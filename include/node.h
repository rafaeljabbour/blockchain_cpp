#ifndef NODE_H
#define NODE_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "peer.h"
#include "server.h"

// maximum number of simultaneous peer connections
inline constexpr size_t MAX_PEERS = 125;

// tracks a peer connection and its handshake state
struct PeerState {
        std::unique_ptr<Peer> peer;
        bool versionSent = false;      // have we sent our version to this peer?
        bool versionReceived = false;  // have we received their version?
        bool handshakeComplete = false;
        int32_t remoteHeight = -1;  // their blockchain height

        explicit PeerState(std::unique_ptr<Peer> p) : peer(std::move(p)) {}
};

class Node {
    private:
        uint16_t port;
        std::string ip;
        Server server;
        std::atomic<bool> running;

        std::vector<std::shared_ptr<PeerState>> peers;
        std::mutex peersMutex;
        std::vector<std::thread> peerThreads;

        void StartPeerLoop(std::shared_ptr<PeerState> peerState);

        void DispatchMessage(PeerState& peerState, const Message& msg);

        // message handlers
        void HandleVersion(PeerState& peerState, const std::vector<uint8_t>& payload);
        void HandleVerack(PeerState& peerState);

        // sends our version message to a peer
        void SendVersion(PeerState& peerState);

        int32_t GetBlockchainHeight() const;

    public:
        Node(const std::string& ip, uint16_t port);
        ~Node();

        Node(const Node&) = delete;
        Node& operator=(const Node&) = delete;

        // connects to a seed node and initiates handshake
        void ConnectToSeed(const std::string& seedIP, uint16_t seedPort);

        // starts listening and/or connects to a seed node
        void Start(const std::string& seedAddr = "");

        void Stop();
};

#endif