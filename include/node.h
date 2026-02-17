#ifndef NODE_H
#define NODE_H

#include <atomic>
#include <condition_variable>
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

// liveliness monitoring constants
inline constexpr int PING_INTERVAL_SECS = 120;
inline constexpr int PING_TIMEOUT_SECS = 30;

// tracks a peer connection, handshake state, and liveliness
struct PeerState {
        std::unique_ptr<Peer> peer;
        bool versionSent = false;      // have we sent our version to this peer?
        bool versionReceived = false;  // have we received their version?
        bool handshakeComplete = false;
        int32_t remoteHeight = -1;    // their blockchain height
        uint64_t services = 0;        // services they advertise
        std::string userAgent;        // their software name/version
        int32_t protocolVersion = 0;  // their protocol version

        // liveliness monitoring
        std::mutex pongMutex;
        std::condition_variable pongCV;
        uint64_t pongNonce = 0;
        bool pongReceived = false;

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
        void HandlePing(PeerState& peerState, const std::vector<uint8_t>& payload);
        void HandlePong(PeerState& peerState, const std::vector<uint8_t>& payload);
        void HandleInv(PeerState& peerState, const std::vector<uint8_t>& payload);
        void HandleTx(PeerState& peerState, const std::vector<uint8_t>& payload);

        void SendVersion(PeerState& peerState);

        void MonitorPeer(std::shared_ptr<PeerState> peerState);

        void DisconnectPeer(const std::string& peerAddr);

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