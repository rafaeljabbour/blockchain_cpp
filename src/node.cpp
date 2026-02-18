#include "node.h"

#include <chrono>
#include <iostream>
#include <stdexcept>

#include "blockchain.h"
#include "blockchainIterator.h"
#include "message.h"
#include "messageInv.h"
#include "messagePing.h"
#include "messageVerack.h"
#include "messageVersion.h"
#include "proofOfWork.h"
#include "transaction.h"
#include "utils.h"

Node::Node(const std::string& ip, uint16_t port, uint16_t rpcPort)
    : port(port),
      ip(ip),
      server(port),
      running(false),
      blockchainHeight(ComputeBlockchainHeight()),
      rpcServer(rpcPort) {
    RegisterRPCMethods();
}

Node::~Node() { Stop(); }

int32_t Node::ComputeBlockchainHeight() {
    try {
        if (!Blockchain::DBExists()) {
            return -1;
        }

        Blockchain bc;
        BlockchainIterator bci = bc.Iterator();

        int32_t height = 0;
        while (bci.hasNext()) {
            bci.Next();
            height++;
        }

        return height;
    } catch (const std::exception&) {
        return -1;
    }
}

bool Node::VerifyTransaction(const Transaction& tx) {
    if (tx.IsCoinbase()) {
        return true;
    }

    // at least one input
    if (tx.GetVin().empty()) {
        return false;
    }

    // at least one output
    if (tx.GetVout().empty()) {
        return false;
    }

    return true;
}

void Node::RegisterRPCMethods() {
    rpcServer.RegisterMethod("getmempool", [this]() -> nlohmann::json {
        auto ids = mempool.GetTransactionIDs();
        nlohmann::json result;
        result["size"] = ids.size();
        result["transactions"] = std::move(ids);

        return result;
    });
}

void Node::SendVersion(PeerState& peerState) {
    MessageVersion version(peerState.peer->GetRemoteIP(), peerState.peer->GetRemotePort(), ip, port,
                           blockchainHeight, true);

    Message msg(MAGIC_CUSTOM, CMD_VERSION, version.Serialize());
    peerState.peer->SendMessage(msg);
    peerState.versionSent = true;

    std::cout << "[node] Sent version (height=" << blockchainHeight << ") to "
              << peerState.peer->GetRemoteAddress() << std::endl;
}

void Node::HandleVersion(PeerState& peerState, const std::vector<uint8_t>& payload) {
    MessageVersion remoteVersion = MessageVersion::Deserialize(payload);

    peerState.versionReceived = true;
    peerState.remoteHeight = remoteVersion.GetStartHeight();
    peerState.services = remoteVersion.GetServices();
    peerState.userAgent = remoteVersion.GetUserAgent();
    peerState.protocolVersion = remoteVersion.GetVersion();

    std::cout << "[node] Received version from " << peerState.peer->GetRemoteAddress()
              << " (height=" << remoteVersion.GetStartHeight()
              << ", agent=" << remoteVersion.GetUserAgent() << ")" << std::endl;

    // send version if we're receiving side
    if (!peerState.versionSent) {
        SendVersion(peerState);
    }

    // acknowledge their version
    peerState.peer->SendMessage(CreateVerackMessage());
    std::cout << "[node] Sent verack to " << peerState.peer->GetRemoteAddress() << std::endl;

    // compare heights for sync
    if (remoteVersion.GetStartHeight() > blockchainHeight) {
        std::cout << "[node] Peer " << peerState.peer->GetRemoteAddress() << " has more blocks ("
                  << remoteVersion.GetStartHeight() << " vs our " << blockchainHeight << ")"
                  << std::endl;
        // TODO: Have to implement request blocks from this peer
    } else if (remoteVersion.GetStartHeight() < blockchainHeight) {
        std::cout << "[node] We have more blocks than " << peerState.peer->GetRemoteAddress()
                  << " (" << blockchainHeight << " vs their " << remoteVersion.GetStartHeight()
                  << ")" << std::endl;
        // TODO: also have to implement if they will request blocks from us
    } else {
        std::cout << "[node] Same height as " << peerState.peer->GetRemoteAddress() << std::endl;
    }
}

void Node::HandleVerack(PeerState& peerState) {
    peerState.handshakeComplete = true;
    std::cout << "[node] Handshake complete with " << peerState.peer->GetRemoteAddress()
              << std::endl;
}

void Node::HandlePing(PeerState& peerState, const std::vector<uint8_t>& payload) {
    MessagePing ping = MessagePing::Deserialize(payload);

    // immediately echo back the nonce in a pong
    Message pong = CreatePongMessage(ping.GetNonce());
    peerState.peer->SendMessage(pong);

    std::cout << "[node] Replied pong to " << peerState.peer->GetRemoteAddress() << std::endl;
}

void Node::HandlePong(PeerState& peerState, const std::vector<uint8_t>& payload) {
    MessagePong pong = MessagePong::Deserialize(payload);

    // signal the monitor thread that a pong was received
    {
        std::lock_guard<std::mutex> lock(peerState.pongMutex);
        peerState.pongNonce = pong.GetNonce();
        peerState.pongReceived = true;
    }
    peerState.pongCV.notify_one();
}

void Node::HandleInv(PeerState& peerState, const std::vector<uint8_t>& payload) {
    MessageInv inv = MessageInv::Deserialize(payload);

    std::cout << "[node] Received inv with " << +inv.GetCount() << " items from "
              << peerState.peer->GetRemoteAddress() << std::endl;

    // reply with getdata for all announced objects
    MessageGetData getData(inv.GetInventory());
    Message msg(MAGIC_CUSTOM, CMD_GETDATA, getData.Serialize());
    peerState.peer->SendMessage(msg);

    std::cout << "[node] Sent getdata for " << +getData.GetCount() << " items to "
              << peerState.peer->GetRemoteAddress() << std::endl;
}

void Node::HandleTx(PeerState& peerState, const std::vector<uint8_t>& payload) {
    try {
        Transaction tx = Transaction::Deserialize(payload);
        std::string txid = ByteArrayToHexString(tx.GetID());

        std::cout << "[node] Received transaction " << txid << " from "
                  << peerState.peer->GetRemoteAddress() << std::endl;

        // verification before the mempool insertion
        if (!VerifyTransaction(tx)) {
            std::cerr << "[node] Rejected invalid transaction " << txid << std::endl;
            return;
        }

        mempool.AddTransaction(tx);
    } catch (const std::exception& e) {
        std::cerr << "[node] Failed to deserialize tx from " << peerState.peer->GetRemoteAddress()
                  << ": " << e.what() << std::endl;
    }
}

void Node::HandleBlock(PeerState& peerState, const std::vector<uint8_t>& payload) {
    try {
        Block block = Block::Deserialize(payload);
        std::string blockHash = ByteArrayToHexString(block.GetHash());

        std::cout << "[node] Received block " << blockHash << " from "
                  << peerState.peer->GetRemoteAddress() << std::endl;

        // verify proof of work
        ProofOfWork pow(&block);
        if (!pow.Validate()) {
            std::cerr << "[node] Rejected invalid block " << blockHash << std::endl;
            return;
        }

        std::cout << "[node] Block " << blockHash << " passed PoW verification" << std::endl;

        // validate all transactions in the block
        for (const auto& tx : block.GetTransactions()) {
            if (!VerifyTransaction(tx)) {
                std::cerr << "[node] Rejected block " << blockHash
                          << ": contains invalid transaction " << ByteArrayToHexString(tx.GetID())
                          << std::endl;
                return;
            }
        }

        // remove mined transactions from mempool
        mempool.RemoveBlockTransactions(block);

        // update cached height
        blockchainHeight++;
        std::cout << "[node] Blockchain height is now " << blockchainHeight << std::endl;

        // TODO: store the block in the blockchain database
    } catch (const std::exception& e) {
        std::cerr << "[node] Failed to process block from " << peerState.peer->GetRemoteAddress()
                  << ": " << e.what() << std::endl;
    }
}

void Node::DispatchMessage(PeerState& peerState, const Message& msg) {
    std::string cmd = msg.GetCommandString();

    if (cmd == CMD_VERSION) {
        HandleVersion(peerState, msg.GetPayload());
    } else if (cmd == CMD_VERACK) {
        HandleVerack(peerState);
    } else if (cmd == CMD_PING) {
        HandlePing(peerState, msg.GetPayload());
    } else if (cmd == CMD_PONG) {
        HandlePong(peerState, msg.GetPayload());
    } else if (cmd == CMD_INV) {
        HandleInv(peerState, msg.GetPayload());
    } else if (cmd == CMD_TX) {
        HandleTx(peerState, msg.GetPayload());
    } else if (cmd == CMD_BLOCK) {
        HandleBlock(peerState, msg.GetPayload());
    } else {
        std::cout << "[node] Unknown command '" << cmd << "' from "
                  << peerState.peer->GetRemoteAddress() << std::endl;
    }
}

void Node::MonitorPeer(std::shared_ptr<PeerState> peerState) {
    while (running && peerState->peer->IsConnected()) {
        // wait before first ping
        std::this_thread::sleep_for(std::chrono::seconds(PING_INTERVAL_SECS));

        if (!running || !peerState->peer->IsConnected()) {
            break;
        }

        // send ping with random nonce
        auto [pingMsg, nonce] = CreatePingMessage();

        try {
            peerState->peer->SendMessage(pingMsg);
        } catch (const std::exception& e) {
            std::cerr << "[node] Failed to send ping to " << peerState->peer->GetRemoteAddress()
                      << ": " << e.what() << std::endl;
            DisconnectPeer(peerState->peer->GetRemoteAddress());
            return;
        }

        std::cout << "[node] Sent ping to " << peerState->peer->GetRemoteAddress() << std::endl;

        // wait for pong with timeout
        {
            std::unique_lock<std::mutex> lock(peerState->pongMutex);
            peerState->pongReceived = false;

            bool received =
                peerState->pongCV.wait_for(lock, std::chrono::seconds(PING_TIMEOUT_SECS),
                                           [&peerState]() { return peerState->pongReceived; });

            if (!received) {
                std::cerr << "[node] Peer " << peerState->peer->GetRemoteAddress()
                          << " no pong reply for " << PING_TIMEOUT_SECS << "s -- disconnecting"
                          << std::endl;
                DisconnectPeer(peerState->peer->GetRemoteAddress());
                return;
            }

            // validate nonce matches
            if (peerState->pongNonce != nonce) {
                std::cerr << "[node] Nonce mismatch from " << peerState->peer->GetRemoteAddress()
                          << ": expected " << nonce << ", got " << peerState->pongNonce
                          << " -- disconnecting" << std::endl;
                DisconnectPeer(peerState->peer->GetRemoteAddress());
                return;
            }
        }

        std::cout << "[node] Got pong from " << peerState->peer->GetRemoteAddress() << std::endl;
    }
}

void Node::DisconnectPeer(const std::string& peerAddr) {
    std::cout << "[node] Disconnecting peer " << peerAddr << std::endl;

    std::lock_guard<std::mutex> lock(peersMutex);
    for (auto& peerState : peers) {
        if (peerState->peer->GetRemoteAddress() == peerAddr) {
            peerState->peer->Disconnect();

            // wake monitor thread so it exits
            {
                std::lock_guard<std::mutex> pongLock(peerState->pongMutex);
                peerState->pongReceived = true;
            }
            peerState->pongCV.notify_one();
            return;
        }
    }
}

void Node::CleanupDisconnectedPeers() {
    // collect disconnected peers under lock, then join their threads outside the lock
    std::vector<std::shared_ptr<PeerState>> toCleanup;

    {
        std::lock_guard<std::mutex> lock(peersMutex);
        auto it = peers.begin();
        while (it != peers.end()) {
            if (!(*it)->peer->IsConnected()) {
                toCleanup.push_back(*it);
                it = peers.erase(it);
            } else {
                ++it;
            }
        }
    }

    // join outside the lock
    for (auto& peerState : toCleanup) {
        if (peerState->readerThread.joinable()) {
            peerState->readerThread.join();
        }
        if (peerState->monitorThread.joinable()) {
            peerState->monitorThread.join();
        }
    }

    if (!toCleanup.empty()) {
        std::cout << "[node] Cleaned up " << toCleanup.size() << " disconnected peer(s)"
                  << std::endl;
    }
}

void Node::RunCleanupLoop() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(30));

        if (!running) {
            break;
        }

        CleanupDisconnectedPeers();
    }
}

void Node::StartPeerLoop(std::shared_ptr<PeerState> peerState) {
    // the thread that reads messages
    peerState->readerThread = std::thread([this, peerState]() {
        try {
            while (running && peerState->peer->IsConnected()) {
                Message msg = peerState->peer->ReceiveMessage();
                DispatchMessage(*peerState, msg);
            }
        } catch (const std::exception& e) {
            if (running) {
                std::cerr << "[node] Peer " << peerState->peer->GetRemoteAddress()
                          << " disconnected: " << e.what() << std::endl;
            }
        }

        peerState->peer->Disconnect();

        // wake the monitor thread so it exits
        {
            std::lock_guard<std::mutex> lock(peerState->pongMutex);
            peerState->pongReceived = true;
        }
        peerState->pongCV.notify_one();
    });

    // the thread that monitors liveliness
    peerState->monitorThread = std::thread([this, peerState]() { MonitorPeer(peerState); });
}

void Node::ConnectToSeed(const std::string& seedIP, uint16_t seedPort) {
    try {
        auto peer = ConnectToPeer(seedIP, seedPort);

        auto peerState = std::make_shared<PeerState>(std::move(peer));

        // outbound connection
        SendVersion(*peerState);

        {
            std::lock_guard<std::mutex> lock(peersMutex);
            peers.push_back(peerState);
        }

        // start reading messages and monitoring this peer
        StartPeerLoop(peerState);

    } catch (const std::exception& e) {
        std::cerr << "[node] Failed to connect to seed " << seedIP << ":" << seedPort << ": "
                  << e.what() << std::endl;
    }
}

void Node::Start(const std::string& seedAddr) {
    server.Start();
    running = true;

    std::cout << "[node] Node started on " << ip << ":" << port << std::endl;
    std::cout << "[node] Blockchain height: " << blockchainHeight << std::endl;

    // start the JSON RPC server for query
    rpcServer.Start();

    // start background cleanup of disconnected peers
    cleanupThread = std::thread([this]() { RunCleanupLoop(); });

    // outbound connection (connect to seed node if specified)
    if (!seedAddr.empty()) {
        size_t colonPos = seedAddr.find(':');
        if (colonPos == std::string::npos) {
            throw std::runtime_error("Invalid seed address format. Use IP:PORT");
        }

        std::string seedIP = seedAddr.substr(0, colonPos);
        uint16_t seedPort = static_cast<uint16_t>(std::stoi(seedAddr.substr(colonPos + 1)));

        ConnectToSeed(seedIP, seedPort);
    }

    // blocks until a peer connects, then spawns a handler thread
    while (running) {
        try {
            auto peer = server.AcceptConnection();
            if (!peer) {
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(peersMutex);
                if (peers.size() >= MAX_PEERS) {
                    std::cout << "[node] Max peers reached (" << MAX_PEERS
                              << "), rejecting connection from " << peer->GetRemoteAddress()
                              << std::endl;
                    peer->Disconnect();
                    continue;
                }
            }

            auto peerState = std::make_shared<PeerState>(std::move(peer));

            {
                std::lock_guard<std::mutex> lock(peersMutex);
                peers.push_back(peerState);
            }

            // inbound connection
            StartPeerLoop(peerState);

        } catch (const std::exception& e) {
            if (running) {
                std::cerr << "[node] Accept error: " << e.what() << std::endl;
            }
        }
    }
}

void Node::Stop() {
    running = false;
    server.Stop();
    rpcServer.Stop();

    // join the cleanup thread first
    if (cleanupThread.joinable()) {
        cleanupThread.join();
    }

    // copy peers under lock, disconnect and wake all monitor threads
    std::vector<std::shared_ptr<PeerState>> peersSnapshot;
    {
        std::lock_guard<std::mutex> lock(peersMutex);
        peersSnapshot = peers;

        for (auto& peerState : peersSnapshot) {
            peerState->peer->Disconnect();

            // we wake the monitoring thread so it exits
            {
                std::lock_guard<std::mutex> pongLock(peerState->pongMutex);
                peerState->pongReceived = true;
            }
            peerState->pongCV.notify_one();
        }
    }

    // join all threads
    for (auto& peerState : peersSnapshot) {
        if (peerState->readerThread.joinable()) {
            peerState->readerThread.join();
        }
        if (peerState->monitorThread.joinable()) {
            peerState->monitorThread.join();
        }
    }

    {
        std::lock_guard<std::mutex> lock(peersMutex);
        peers.clear();
    }

    std::cout << "[node] Node stopped" << std::endl;
}