#include "rpcServer.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>

RPCServer::RPCServer(uint16_t port) : port(port), listenSockfd(-1), running(false) {}

RPCServer::~RPCServer() { Stop(); }

void RPCServer::RegisterMethod(const std::string& name, std::function<json(const json&)> handler) {
    methods[name] = std::move(handler);
}

void RPCServer::HandleConnection(int clientfd) {
    // read until newline or connection close
    std::string request;
    char buf[4096];

    while (true) {
        ssize_t n = recv(clientfd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            break;
        }
        buf[n] = '\0';
        request += buf;

        // check for newline delimiter
        if (request.find('\n') != std::string::npos) {
            break;
        }
    }

    if (request.empty()) {
        close(clientfd);
        return;
    }

    json response;

    try {
        json req = json::parse(request);

        // extract fields per JSON-RPC 2.0
        auto id = req.value("id", json(nullptr));
        std::string method = req.value("method", "");

        // extract params and normalise empty array to empty object
        json params = req.value("params", json::object());
        if (params.is_array() && params.empty()) {
            params = json::object();
        }

        auto it = methods.find(method);
        if (it != methods.end()) {
            json result = it->second(params);
            response = {{"jsonrpc", "2.0"}, {"result", result}, {"id", id}};
        } else {
            response = {{"jsonrpc", "2.0"},
                        {"error", {{"code", -32601}, {"message", "Method not found: " + method}}},
                        {"id", id}};
        }
    } catch (const json::parse_error& e) {
        response = {{"jsonrpc", "2.0"},
                    {"error", {{"code", -32700}, {"message", "Parse error"}}},
                    {"id", nullptr}};
    } catch (const std::exception& e) {
        response = {
            {"jsonrpc", "2.0"},
            {"error", {{"code", -32603}, {"message", std::string("Internal error: ") + e.what()}}},
            {"id", nullptr}};
    }

    // serialize and send
    std::string responseStr = response.dump() + "\n";

    size_t totalSent = 0;
    while (totalSent < responseStr.size()) {
        ssize_t n =
            send(clientfd, responseStr.data() + totalSent, responseStr.size() - totalSent, 0);
        if (n <= 0) {
            break;
        }
        totalSent += static_cast<size_t>(n);
    }

    close(clientfd);
}

void RPCServer::AcceptLoop() {
    while (running) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);

        int clientfd = accept(listenSockfd, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (clientfd < 0) {
            if (running) {
                std::cerr << "[rpc] Accept error: " << strerror(errno) << std::endl;
            }
            continue;
        }

        // set a timeout
        timeval tv{};
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(clientfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(clientfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        HandleConnection(clientfd);
    }
}

void RPCServer::Start() {
    listenSockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSockfd < 0) {
        throw std::runtime_error("Failed to create RPC socket");
    }

    // allow port reuse
    int opt = 1;
    if (setsockopt(listenSockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(listenSockfd);
        listenSockfd = -1;
        throw std::runtime_error("Failed to set SO_REUSEADDR");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    // localhost only for security
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (bind(listenSockfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(listenSockfd);
        listenSockfd = -1;
        throw std::runtime_error("Failed to bind RPC server on port " + std::to_string(port));
    }

    if (listen(listenSockfd, 5) < 0) {
        close(listenSockfd);
        listenSockfd = -1;
        throw std::runtime_error("Failed to listen on RPC port " + std::to_string(port));
    }

    running = true;
    serverThread = std::thread([this]() { AcceptLoop(); });

    std::cout << "[rpc] JSON-RPC server listening on 127.0.0.1:" << port << std::endl;
}

void RPCServer::Stop() {
    running = false;

    if (listenSockfd >= 0) {
        close(listenSockfd);
        listenSockfd = -1;
    }

    if (serverThread.joinable()) {
        serverThread.join();
    }
}

json RPCCall(uint16_t port, const std::string& method, const json& params) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        throw std::runtime_error("Failed to create RPC client socket");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (connect(sockfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(sockfd);
        throw std::runtime_error("Failed to connect to RPC server on port " + std::to_string(port));
    }

    // set timeout
    timeval tv{};
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // build and send JSON-RPC request
    json request = {{"jsonrpc", "2.0"}, {"method", method}, {"params", params}, {"id", 1}};

    std::string requestStr = request.dump() + "\n";

    size_t totalSent = 0;
    while (totalSent < requestStr.size()) {
        ssize_t n = send(sockfd, requestStr.data() + totalSent, requestStr.size() - totalSent, 0);
        if (n <= 0) {
            close(sockfd);
            throw std::runtime_error("Failed to send RPC request");
        }
        totalSent += static_cast<size_t>(n);
    }

    // read response
    std::string responseStr;
    char buf[4096];
    while (true) {
        ssize_t n = recv(sockfd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            break;
        }
        buf[n] = '\0';
        responseStr += buf;

        if (responseStr.find('\n') != std::string::npos) {
            break;
        }
    }

    close(sockfd);

    // parse response
    json response = json::parse(responseStr);

    if (response.contains("error") && !response["error"].is_null()) {
        std::string errMsg = response["error"].value("message", "Unknown error");
        throw std::runtime_error("RPC error: " + errMsg);
    }

    return response["result"];
}