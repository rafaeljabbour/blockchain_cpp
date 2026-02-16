#ifndef PEER_H
#define PEER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "message.h"

// default receive timeout in seconds
inline constexpr int PEER_RECV_TIMEOUT_SECS = 90;

// represents a single TCP connection to another node.
class Peer {
    private:
        int sockfd;
        std::string remoteIP;
        uint16_t remotePort;
        bool connected;

        std::vector<uint8_t> ReadExact(size_t count);
        void WriteAll(const std::vector<uint8_t>& data);
        void SetRecvTimeout(int seconds);

    public:
        Peer(int sockfd, const std::string& remoteIP, uint16_t remotePort);
        ~Peer();

        Peer(const Peer&) = delete;
        Peer& operator=(const Peer&) = delete;

        // allow moving
        Peer(Peer&& other) noexcept;
        Peer& operator=(Peer&& other) noexcept;

        void SendMessage(const Message& msg);
        Message ReceiveMessage();

        void Disconnect();

        bool IsConnected() const { return connected; }
        const std::string& GetRemoteIP() const { return remoteIP; }
        uint16_t GetRemotePort() const { return remotePort; }
        std::string GetRemoteAddress() const;
};

std::unique_ptr<Peer> ConnectToPeer(const std::string& ip, uint16_t port);

#endif