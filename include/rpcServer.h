#ifndef RPCSERVER_H
#define RPCSERVER_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

using json = nlohmann::json;

// the default JSON-RPC port
inline constexpr uint16_t DEFAULT_RPC_PORT = 9334;

// JSON-RPC server that exposes node state over TCP (https://www.jsonrpc.org/specification).
class RPCServer {
    private:
        uint16_t port;              // port RPC server listens on
        int listenSockfd;           // socket that waits for incoming RPC connections
        std::atomic<bool> running;  // flag for server loop
        std::thread serverThread;   // thread that runs accept loop

        // maps method names to handler functions
        std::map<std::string, std::function<json(const json&)>> methods;

        void AcceptLoop();
        void HandleConnection(int clientfd);

    public:
        explicit RPCServer(uint16_t port);
        ~RPCServer();

        RPCServer(const RPCServer&) = delete;
        RPCServer& operator=(const RPCServer&) = delete;

        // registers an RPC method by name, with optional parameters
        void RegisterMethod(const std::string& name, std::function<json(const json&)> handler);

        void Start();
        void Stop();
};

// connects to localhost on the given port and calls method with optional parameters
json RPCCall(uint16_t port, const std::string& method, const json& params = json::object());

#endif