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

#include "blockchain.h"
#include "mempool.h"
#include "peer.h"
#include "rpcServer.h"
#include "server.h"

// maximum number of simultaneous peer connections
inline constexpr size_t MAX_PEERS = 125;

// liveliness monitoring constants
inline constexpr int PING_INTERVAL_SECS = 120;
inline constexpr int PING_TIMEOUT_SECS = 30;

// timeout for the miner's condition variable just incase we miss a notification
inline constexpr int MINER_CV_TIMEOUT_SECS = 60;

// tracks a peer connection, handshake state, and liveliness and thread
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

        // threads for liveliness
        std::thread readerThread;
        std::thread monitorThread;

        explicit PeerState(std::unique_ptr<Peer> p) : peer(std::move(p)) {}
};

class Node {
    private:
        uint16_t port;
        std::string ip;
        Server server;
        std::atomic<bool> running;
        std::atomic<int32_t> blockchainHeight;

        Mempool mempool;
        RPCServer rpcServer;

        // persistent blockchain handling
        std::unique_ptr<Blockchain> blockchain;
        std::mutex blockchainMutex;

        std::atomic<bool> syncing{false};
        // protected by blockchainMutex
        std::string syncPeerAddr;

        std::vector<std::shared_ptr<PeerState>> peers;
        std::mutex peersMutex;

        void StartPeerLoop(std::shared_ptr<PeerState> peerState);

        void DispatchMessage(PeerState& peerState, const Message& msg);

        // message handlers
        void HandleVersion(PeerState& peerState, const std::vector<uint8_t>& payload);
        void HandleVerack(PeerState& peerState);
        void HandlePing(PeerState& peerState, const std::vector<uint8_t>& payload);
        void HandlePong(PeerState& peerState, const std::vector<uint8_t>& payload);
        void HandleInv(PeerState& peerState, const std::vector<uint8_t>& payload);
        void HandleTx(PeerState& peerState, const std::vector<uint8_t>& payload);
        void HandleBlock(PeerState& peerState, const std::vector<uint8_t>& payload);
        void HandleGetBlocks(PeerState& peerState, const std::vector<uint8_t>& payload);
        void HandleGetData(PeerState& peerState, const std::vector<uint8_t>& payload);

        void SendVersion(PeerState& peerState);

        void RelayTransaction(const Transaction& tx, const std::string& sourcePeerAddr);

        void MonitorPeer(std::shared_ptr<PeerState> peerState);

        void DisconnectPeer(const std::string& peerAddr);

        void CleanupDisconnectedPeers();

        std::mutex cleanupCVMtx;
        std::condition_variable cleanupCV;
        void RunCleanupLoop(std::stop_token stoken);
        std::jthread cleanupThread;

        // mining
        std::string minerAddress;
        std::jthread minerThread;
        std::mutex minerCVMtx;
        std::condition_variable minerCV;
        void RunMinerLoop(std::stop_token stoken);

        void BroadcastBlock(const Block& block);

        void RegisterRPCMethods();

        static bool VerifyTransaction(const Transaction& tx);

    public:
        Node(const std::string& ip, uint16_t port, uint16_t rpcPort = DEFAULT_RPC_PORT,
             const std::string& minerAddress = "");
        ~Node();

        Node(const Node&) = delete;
        Node& operator=(const Node&) = delete;

        // connects to a seed node and initiates handshake
        void ConnectToSeed(const std::string& seedIP, uint16_t seedPort);

        // starts listening and/or connects to a seed node
        void Start(const std::string& seedAddr = "");

        // when a transaction is created locally on this node, it is broadcast to all peers
        void BroadcastTransaction(const Transaction& tx);

        // mine one block from the current mempool, store it, and relay to peers
        void MineBlock(const std::string& address);

        void Stop();
};

#endif