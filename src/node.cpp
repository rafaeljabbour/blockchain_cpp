#include "node.h"

#include <iostream>
#include <stdexcept>

#include "blockchain.h"
#include "blockchainIterator.h"
#include "message.h"
#include "messageVerack.h"
#include "messageVersion.h"

Node::Node(const std::string& ip, uint16_t port)
    : port(port), ip(ip), server(port), running(false) {}

Node::~Node() { Stop(); }

int32_t Node::GetBlockchainHeight() const {
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

void Node::SendVersion(PeerState& peerState) {
    int32_t height = GetBlockchainHeight();

    MessageVersion version(peerState.peer->GetRemoteIP(), peerState.peer->GetRemotePort(), ip, port,
                           height, true);

    Message msg(MAGIC_CUSTOM, CMD_VERSION, version.Serialize());
    peerState.peer->SendMessage(msg);
    peerState.versionSent = true;

    std::cout << "[node] Sent version (height=" << height << ") to "
              << peerState.peer->GetRemoteAddress() << std::endl;
}

void Node::HandleVersion(PeerState& peerState, const std::vector<uint8_t>& payload) {
    MessageVersion remoteVersion = MessageVersion::Deserialize(payload);

    peerState.versionReceived = true;
    peerState.remoteHeight = remoteVersion.GetStartHeight();

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
    int32_t ourHeight = GetBlockchainHeight();
    if (remoteVersion.GetStartHeight() > ourHeight) {
        std::cout << "[node] Peer " << peerState.peer->GetRemoteAddress() << " has more blocks ("
                  << remoteVersion.GetStartHeight() << " vs our " << ourHeight << ")" << std::endl;
        // TODO: Have to implement request blocks from this peer
    } else if (remoteVersion.GetStartHeight() < ourHeight) {
        std::cout << "[node] We have more blocks than " << peerState.peer->GetRemoteAddress()
                  << " (" << ourHeight << " vs their " << remoteVersion.GetStartHeight() << ")"
                  << std::endl;
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

void Node::DispatchMessage(PeerState& peerState, const Message& msg) {
    std::string cmd = msg.GetCommandString();

    if (cmd == CMD_VERSION) {
        HandleVersion(peerState, msg.GetPayload());
    } else if (cmd == CMD_VERACK) {
        HandleVerack(peerState);
    } else {
        std::cout << "[node] Unknown command '" << cmd << "' from "
                  << peerState.peer->GetRemoteAddress() << std::endl;
    }
}

void Node::StartPeerLoop(std::shared_ptr<PeerState> peerState) {
    peerThreads.emplace_back([this, peerState]() {
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
    });
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

        // start reading messages from this peer
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
    std::cout << "[node] Blockchain height: " << GetBlockchainHeight() << std::endl;

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

    // disconnect all the peers
    {
        std::lock_guard<std::mutex> lock(peersMutex);
        for (auto& peerState : peers) {
            peerState->peer->Disconnect();
        }
    }

    // wait for all the threads to finish
    for (auto& thread : peerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    peerThreads.clear();

    {
        std::lock_guard<std::mutex> lock(peersMutex);
        peers.clear();
    }

    std::cout << "[node] Node stopped" << std::endl;
}