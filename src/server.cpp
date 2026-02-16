#include "server.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <stdexcept>

Server::Server(uint16_t port) : listenSockfd(-1), port(port), running(false) {}

Server::~Server() { Stop(); }

void Server::Start() {
    listenSockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSockfd < 0) {
        throw std::runtime_error("Failed to create listen socket");
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
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listenSockfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(listenSockfd);
        listenSockfd = -1;
        throw std::runtime_error("Failed to bind to port " + std::to_string(port));
    }

    // listen for connections
    if (listen(listenSockfd, 10) < 0) {
        close(listenSockfd);
        listenSockfd = -1;
        throw std::runtime_error("Failed to listen on port " + std::to_string(port));
    }

    running = true;
    std::cout << "[net] Listening on port " << port << std::endl;
}

void Server::Stop() {
    running = false;
    if (listenSockfd >= 0) {
        close(listenSockfd);
        listenSockfd = -1;
    }
}

std::unique_ptr<Peer> Server::AcceptConnection() {
    if (!running) {
        throw std::runtime_error("Server is not running");
    }

    sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);

    int clientSockfd = accept(listenSockfd, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
    if (clientSockfd < 0) {
        // only error if we're still supposed to be running
        if (running) {
            throw std::runtime_error("Failed to accept connection");
        }
        return nullptr;
    }

    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
    uint16_t clientPort = ntohs(clientAddr.sin_port);

    std::cout << "[net] Accepted connection from " << clientIP << ":" << clientPort << std::endl;

    return std::make_unique<Peer>(clientSockfd, clientIP, clientPort);
}