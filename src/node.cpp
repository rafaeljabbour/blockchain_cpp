#include "node.h"

#include <chrono>
#include <iostream>
#include <stdexcept>

#include "blockchain.h"
#include "blockchainIterator.h"
#include "message.h"
#include "messageGetBlocks.h"
#include "messageInv.h"
#include "messagePing.h"
#include "messageVerack.h"
#include "messageVersion.h"
#include "proofOfWork.h"
#include "transaction.h"
#include "utils.h"
#include "utxoSet.h"
#include "wallet.h"

using json = nlohmann::json;

Node::Node(const std::string& ip, uint16_t port, uint16_t rpcPort, const std::string& minerAddress)
    : port(port),
      ip(ip),
      server(port),
      running(false),
      blockchainHeight(ComputeBlockchainHeight()),
      rpcServer(rpcPort),
      minerAddress(minerAddress) {
    // open persistent blockchain handle if the database exists
    if (Blockchain::DBExists()) {
        try {
            blockchain = std::make_unique<Blockchain>();
        } catch (const std::exception& e) {
            std::cerr << "[node] Warning: could not open blockchain: " << e.what() << std::endl;
        }
    }

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
    rpcServer.RegisterMethod("getmempool", [this](const json&) -> json {
        auto ids = mempool.GetTransactionIDs();
        json result;
        result["size"] = ids.size();
        result["transactions"] = std::move(ids);

        return result;
    });

    rpcServer.RegisterMethod("getblockcount",
                             [this](const json&) -> json { return blockchainHeight.load(); });

    rpcServer.RegisterMethod("getsyncing", [this](const json&) -> json {
        json result;
        result["syncing"] = syncing.load();
        result["height"] = blockchainHeight.load();
        if (syncing) {
            std::lock_guard<std::mutex> lock(blockchainMutex);
            result["syncPeer"] = syncPeerAddr;
        }
        return result;
    });

    // build and submit a transaction from a local wallet address to the mempool
    rpcServer.RegisterMethod("sendtx", [this](const json& params) -> json {
        std::string from = params.value("from", "");
        std::string to = params.value("to", "");
        int amount = params.value("amount", 0);

        if (from.empty()) throw std::runtime_error("Missing 'from' parameter");
        if (to.empty()) throw std::runtime_error("Missing 'to' parameter");
        if (amount <= 0) throw std::runtime_error("'amount' must be positive");

        if (!Wallet::ValidateAddress(from)) throw std::runtime_error("Invalid 'from' address");
        if (!Wallet::ValidateAddress(to)) throw std::runtime_error("Invalid 'to' address");

        Transaction tx;
        {
            std::lock_guard<std::mutex> lock(blockchainMutex);
            if (!blockchain) throw std::runtime_error("No blockchain available");
            UTXOSet utxoSet(blockchain.get());
            tx = Transaction::NewUTXOTransaction(from, to, amount, &utxoSet);
        }

        std::string txid = ByteArrayToHexString(tx.GetID());

        if (mempool.Contains(txid)) {
            return json{{"txid", txid}, {"status", "already in mempool"}};
        }

        mempool.AddTransaction(tx);
        minerCV.notify_one();
        RelayTransaction(tx, "");

        std::cout << "[rpc] sendtx: submitted tx " << txid << std::endl;
        return json{{"txid", txid}};
    });

    // mine one block from the current mempool on demand
    rpcServer.RegisterMethod("mine", [this](const json& params) -> json {
        std::string address = params.value("address", "");
        if (address.empty()) throw std::runtime_error("Missing 'address' parameter");
        if (!Wallet::ValidateAddress(address)) throw std::runtime_error("Invalid miner address");

        MineBlock(address);

        std::string tipHash;
        {
            std::lock_guard<std::mutex> lock(blockchainMutex);
            if (blockchain) tipHash = ByteArrayToHexString(blockchain->GetTip());
        }

        return json{{"hash", tipHash}, {"height", blockchainHeight.load()}};
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

    // compare heights and initiate sync if behind
    if (remoteVersion.GetStartHeight() > blockchainHeight) {
        std::cout << "[node] Peer " << peerState.peer->GetRemoteAddress() << " has more blocks ("
                  << remoteVersion.GetStartHeight() << " vs our " << blockchainHeight << ")"
                  << std::endl;

        if (!syncing && blockchain) {
            Message getBlocksMsg;
            bool shouldSync = false;
            {
                std::lock_guard<std::mutex> lock(blockchainMutex);
                // check again under lock to prevent race between different handlers
                if (!syncing && blockchain) {
                    syncing = true;
                    syncPeerAddr = peerState.peer->GetRemoteAddress();

                    MessageGetBlocks getBlocks(blockchain->GetTip());
                    getBlocksMsg = Message(MAGIC_CUSTOM, CMD_GETBLOCKS, getBlocks.Serialize());
                    shouldSync = true;
                }
            }

            if (shouldSync) {
                peerState.peer->SendMessage(getBlocksMsg);
                std::cout << "[node] Sent getblocks to " << peerState.peer->GetRemoteAddress()
                          << std::endl;
            }
        }
    } else if (remoteVersion.GetStartHeight() < blockchainHeight) {
        std::cout << "[node] We have more blocks than " << peerState.peer->GetRemoteAddress()
                  << " (" << blockchainHeight << " vs their " << remoteVersion.GetStartHeight()
                  << ")" << std::endl;
        // peer will request blocks from us using getblocks
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

    // only request objects we don't already have
    std::vector<InvVector> toRequest;
    toRequest.reserve(inv.GetInventory().size());

    for (const auto& item : inv.GetInventory()) {
        // if item is a transaction, check if we already have it
        if (item.type == InvType::Tx) {
            std::string txid = ByteArrayToHexString(item.hash);
            // check if we already have it
            if (!mempool.Contains(txid)) {
                toRequest.push_back(item);
            } else {
                std::cout << "[node] Already have tx " << txid.substr(0, 16) << "..., skipping"
                          << std::endl;
            }
        }
        // if item is a block, add it to the request list without checking if we already have it
        else {
            toRequest.push_back(item);
        }
    }

    if (toRequest.empty()) {
        return;
    }

    MessageGetData getData(toRequest);
    Message msg(MAGIC_CUSTOM, CMD_GETDATA, getData.Serialize());
    peerState.peer->SendMessage(msg);

    std::cout << "[node] Sent getdata for " << +getData.GetCount() << " items to "
              << peerState.peer->GetRemoteAddress() << std::endl;
}

void Node::HandleGetBlocks(PeerState& peerState, const std::vector<uint8_t>& payload) {
    try {
        MessageGetBlocks getBlocks = MessageGetBlocks::Deserialize(payload);

        // gather hashes under lock
        std::vector<std::vector<uint8_t>> hashes;
        bool noCommonAncestor = false;
        {
            std::lock_guard<std::mutex> lock(blockchainMutex);
            if (!blockchain) {
                std::cerr << "[node] Cannot handle getblocks: no blockchain" << std::endl;
                return;
            }

            hashes = blockchain->GetBlockHashesAfter(getBlocks.GetTipHash());

            if (hashes.empty()) {
                if (getBlocks.GetTipHash() != blockchain->GetTip()) {
                    noCommonAncestor = true;
                }
            }
        }

        if (hashes.empty()) {
            if (noCommonAncestor) {
                std::string peerTip = ByteArrayToHexString(getBlocks.GetTipHash()).substr(0, 16);
                std::cerr << "[node] No common ancestor with " << peerState.peer->GetRemoteAddress()
                          << " (their tip: " << peerTip << "...)" << std::endl;
            } else {
                std::cout << "[node] Peer " << peerState.peer->GetRemoteAddress()
                          << " is already up to date" << std::endl;
            }
            return;
        }

        // build and send inv outside the lock
        std::vector<InvVector> inventory;
        inventory.reserve(hashes.size());
        for (const auto& hash : hashes) {
            inventory.push_back({InvType::Block, hash});
        }

        MessageInv inv(inventory);
        Message msg(MAGIC_CUSTOM, CMD_INV, inv.Serialize());
        peerState.peer->SendMessage(msg);

        std::cout << "[node] Sent inv with " << hashes.size() << " block hashes to "
                  << peerState.peer->GetRemoteAddress() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[node] Failed to handle getblocks from " << peerState.peer->GetRemoteAddress()
                  << ": " << e.what() << std::endl;
    }
}

void Node::HandleGetData(PeerState& peerState, const std::vector<uint8_t>& payload) {
    try {
        MessageGetData getData = MessageGetData::Deserialize(payload);

        std::cout << "[node] Received getdata for " << +getData.GetCount() << " items from "
                  << peerState.peer->GetRemoteAddress() << std::endl;

        // separate requested items by type
        std::vector<std::vector<uint8_t>> blockHashes;
        std::vector<std::vector<uint8_t>> txHashes;

        for (const auto& inv : getData.GetInventory()) {
            if (inv.type == InvType::Block) {
                blockHashes.push_back(inv.hash);
            } else if (inv.type == InvType::Tx) {
                txHashes.push_back(inv.hash);
            }
        }

        // gather all requested blocks under one lock, send outside
        std::vector<Block> blocksToSend;
        {
            std::lock_guard<std::mutex> lock(blockchainMutex);
            if (blockchain) {
                blocksToSend.reserve(blockHashes.size());
                for (const auto& hash : blockHashes) {
                    try {
                        blocksToSend.push_back(blockchain->GetBlock(hash));
                    } catch (const std::exception&) {
                        std::cerr << "[node] Block not found: "
                                  << ByteArrayToHexString(hash).substr(0, 16) << "..." << std::endl;
                    }
                }
            }
        }

        for (const auto& block : blocksToSend) {
            Message msg(MAGIC_CUSTOM, CMD_BLOCK, block.Serialize());
            peerState.peer->SendMessage(msg);

            std::cout << "[node] Sent block " << ByteArrayToHexString(block.GetHash()).substr(0, 16)
                      << "... to " << peerState.peer->GetRemoteAddress() << std::endl;
        }

        // look up each transaction individually
        for (const auto& hash : txHashes) {
            std::string txid = ByteArrayToHexString(hash);
            auto tx = mempool.FindTransaction(txid);
            if (tx) {
                Message msg(MAGIC_CUSTOM, CMD_TX, tx->Serialize());
                peerState.peer->SendMessage(msg);

                std::cout << "[node] Sent tx " << txid.substr(0, 16) << "... to "
                          << peerState.peer->GetRemoteAddress() << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[node] Failed to handle getdata from " << peerState.peer->GetRemoteAddress()
                  << ": " << e.what() << std::endl;
    }
}

void Node::HandleTx(PeerState& peerState, const std::vector<uint8_t>& payload) {
    try {
        Transaction tx = Transaction::Deserialize(payload);
        std::string txid = ByteArrayToHexString(tx.GetID());

        std::cout << "[node] Received transaction " << txid << " from "
                  << peerState.peer->GetRemoteAddress() << std::endl;

        // ignore transactions already in the mempool
        if (mempool.Contains(txid)) {
            std::cout << "[node] Already have tx " << txid.substr(0, 16) << "..., ignoring"
                      << std::endl;
            return;
        }

        if (!VerifyTransaction(tx)) {
            std::cerr << "[node] Rejected invalid transaction " << txid << std::endl;
            return;
        }

        mempool.AddTransaction(tx);
        minerCV.notify_one();

        // flood the inv to all other peers
        RelayTransaction(tx, peerState.peer->GetRemoteAddress());
    } catch (const std::exception& e) {
        std::cerr << "[node] Failed to deserialize tx from " << peerState.peer->GetRemoteAddress()
                  << ": " << e.what() << std::endl;
    }
}

void Node::RelayTransaction(const Transaction& tx, const std::string& sourcePeerAddr) {
    InvVector invVec{InvType::Tx, tx.GetID()};
    MessageInv invMsg({invVec});
    Message msg(MAGIC_CUSTOM, CMD_INV, invMsg.Serialize());

    std::lock_guard<std::mutex> lock(peersMutex);
    for (const auto& peerState : peers) {
        // skip if disconnected but not removed from the list
        if (!peerState->peer->IsConnected()) {
            continue;
        }
        // skip if not exchanged verack
        if (!peerState->handshakeComplete) {
            continue;
        }
        // skip the source peer
        if (peerState->peer->GetRemoteAddress() == sourcePeerAddr) {
            continue;
        }

        try {
            peerState->peer->SendMessage(msg);
            std::cout << "[node] Relayed tx " << ByteArrayToHexString(tx.GetID()).substr(0, 16)
                      << "... inv to " << peerState->peer->GetRemoteAddress() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[node] Failed to relay tx inv to " << peerState->peer->GetRemoteAddress()
                      << ": " << e.what() << std::endl;
        }
    }
}

void Node::BroadcastTransaction(const Transaction& tx) {
    std::string txid = ByteArrayToHexString(tx.GetID());

    if (!VerifyTransaction(tx)) {
        std::cerr << "[node] BroadcastTransaction: rejected invalid transaction " << txid
                  << std::endl;
        return;
    }

    if (!mempool.Contains(txid)) {
        mempool.AddTransaction(tx);
        minerCV.notify_one();
    }

    // empty source
    RelayTransaction(tx, "");
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

        // validate all transactions in the block
        for (const auto& tx : block.GetTransactions()) {
            if (!VerifyTransaction(tx)) {
                std::cerr << "[node] Rejected block " << blockHash
                          << ": contains invalid transaction " << ByteArrayToHexString(tx.GetID())
                          << std::endl;
                return;
            }
        }

        // persist block and check sync status under one lock
        {
            std::lock_guard<std::mutex> lock(blockchainMutex);
            if (!blockchain) {
                std::cerr << "[node] Cannot store block: no blockchain" << std::endl;
                return;
            }

            blockchain->AddBlock(block);

            // remove mined transactions from mempool
            mempool.RemoveBlockTransactions(block);

            // update cached height
            blockchainHeight++;
            std::cout << "[node] Stored block " << blockHash.substr(0, 16)
                      << "... (height=" << blockchainHeight << ")" << std::endl;

            // check if sync is complete
            if (syncing && peerState.peer->GetRemoteAddress() == syncPeerAddr) {
                if (blockchainHeight >= peerState.remoteHeight) {
                    std::cout << "[node] Sync complete! Reindexing UTXO set..." << std::endl;

                    UTXOSet utxoSet(blockchain.get());
                    utxoSet.Reindex();

                    syncing = false;
                    syncPeerAddr.clear();
                    std::cout << "[node] UTXO reindex complete. Chain is up to date at height "
                              << blockchainHeight << std::endl;
                }
            }
        }
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
    } else if (cmd == CMD_GETBLOCKS) {
        HandleGetBlocks(peerState, msg.GetPayload());
    } else if (cmd == CMD_GETDATA) {
        HandleGetData(peerState, msg.GetPayload());
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

void Node::MineBlock(const std::string& address) {
    if (syncing.load()) {
        throw std::runtime_error("Currently syncing, cannot mine");
    }

    // snapshot the mempool
    auto mempoolTxs = mempool.GetTransactions();

    // build the transaction list and read the current tip under the lock
    std::vector<Transaction> txs;
    std::vector<uint8_t> prevHash;
    {
        std::lock_guard<std::mutex> lock(blockchainMutex);
        if (!blockchain) throw std::runtime_error("No blockchain available for mining");

        prevHash = blockchain->GetTip();

        // coinbase reward goes to the miner's address
        txs.push_back(Transaction::NewCoinbaseTX(address, ""));

        for (const auto& [txid, tx] : mempoolTxs) {
            if (blockchain->VerifyTransaction(&tx)) {
                txs.push_back(tx);
            } else {
                std::cerr << "[miner] Dropping invalid tx " << txid.substr(0, 16) << "..."
                          << std::endl;
            }
        }
    }

    std::cout << "[miner] Starting PoW with " << (txs.size() - 1) << " mempool tx(s)..."
              << std::endl;

    // Block constructor runs PoW
    Block minedBlock(txs, prevHash);

    // persist, update UTXO, and clean mempool under the lock
    {
        std::lock_guard<std::mutex> lock(blockchainMutex);
        if (!blockchain) throw std::runtime_error("Blockchain unavailable after mining");

        blockchain->AddBlock(minedBlock);

        UTXOSet utxoSet(blockchain.get());
        utxoSet.Update(minedBlock);

        mempool.RemoveBlockTransactions(minedBlock);
        blockchainHeight++;
    }

    std::string hashStr = ByteArrayToHexString(minedBlock.GetHash());
    std::cout << "[miner] Mined block " << hashStr.substr(0, 16)
              << "... (height=" << blockchainHeight << ")" << std::endl;

    BroadcastBlock(minedBlock);
}

void Node::BroadcastBlock(const Block& block) {
    InvVector invVec{InvType::Block, block.GetHash()};
    MessageInv invMsg({invVec});
    Message msg(MAGIC_CUSTOM, CMD_INV, invMsg.Serialize());

    std::string hashStr = ByteArrayToHexString(block.GetHash());

    std::lock_guard<std::mutex> lock(peersMutex);
    for (const auto& peerState : peers) {
        if (!peerState->peer->IsConnected()) continue;
        if (!peerState->handshakeComplete) continue;

        try {
            peerState->peer->SendMessage(msg);
            std::cout << "[miner] Announced block " << hashStr.substr(0, 16) << "... to "
                      << peerState->peer->GetRemoteAddress() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[miner] Failed to announce block to "
                      << peerState->peer->GetRemoteAddress() << ": " << e.what() << std::endl;
        }
    }
}

void Node::RunMinerLoop() {
    std::cout << "[miner] Background mining thread started (reward → " << minerAddress << ")"
              << std::endl;

    while (running) {
        {
            std::unique_lock<std::mutex> lock(minerCVMtx);
            // sleep until a transaction arrives or the node shuts down, and wakes up just in case
            // every 60 seconds
            minerCV.wait_for(lock, std::chrono::seconds(MINER_CV_TIMEOUT_SECS),
                             [this] { return !running || mempool.GetCount() > 0; });
        }

        if (!running) break;
        if (mempool.GetCount() == 0) continue;

        try {
            std::cout << "[miner] " << mempool.GetCount() << " tx(s) pending, mining..."
                      << std::endl;
            MineBlock(minerAddress);
        } catch (const std::exception& e) {
            // chain may have moved during PoW, so we retry next cycle
            std::cerr << "[miner] Mining cycle error: " << e.what() << std::endl;
        }
    }

    std::cout << "[miner] Background mining thread stopped" << std::endl;
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

    // start background miner if a reward address was configured
    if (!minerAddress.empty()) {
        minerThread = std::thread([this]() { RunMinerLoop(); });
        std::cout << "[node] Background miner enabled (reward → " << minerAddress << ")"
                  << std::endl;
    }

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
    minerCV.notify_all();
    server.Stop();
    rpcServer.Stop();

    // join background threads
    if (cleanupThread.joinable()) {
        cleanupThread.join();
    }
    if (minerThread.joinable()) {
        minerThread.join();
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