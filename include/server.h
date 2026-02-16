#ifndef SERVER_H
#define SERVER_H

#include <cstdint>
#include <memory>

#include "peer.h"

// listens on a TCP port and accepts incoming connections as Peers.
class Server {
    private:
        int listenSockfd;
        uint16_t port;
        bool running;

    public:
        explicit Server(uint16_t port);
        ~Server();

        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;

        void Start();
        void Stop();

        std::unique_ptr<Peer> AcceptConnection();

        bool IsRunning() const { return running; }
        uint16_t GetPort() const { return port; }
};

#endif