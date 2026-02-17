#include "peer.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <iostream>
#include <stdexcept>
#include <utility>

// header size: 4 (magic) + 12 (command) + 4 (length) + 4 (checksum)
static constexpr size_t MESSAGE_HEADER_SIZE = 24;

// reject payloads larger than 32 MB
static constexpr uint32_t MAX_PAYLOAD_SIZE = 32 * 1024 * 1024;

Peer::Peer(int sockfd, const std::string& remoteIP, uint16_t remotePort)
    : sockfd(sockfd), remoteIP(remoteIP), remotePort(remotePort), connected(true) {
    if (PEER_RECV_TIMEOUT_SECS > 0) {
        SetRecvTimeout(PEER_RECV_TIMEOUT_SECS);
    }
    if (PEER_SEND_TIMEOUT_SECS > 0) {
        SetSendTimeout(PEER_SEND_TIMEOUT_SECS);
    }
}

Peer::~Peer() { Disconnect(); }

void Peer::SetRecvTimeout(int seconds) {
    timeval tv{};
    tv.tv_sec = seconds;
    tv.tv_usec = 0;

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        // no need for program to crash
        std::cerr << "[net] Warning: failed to set recv timeout on " << GetRemoteAddress()
                  << std::endl;
    }
}

void Peer::SetSendTimeout(int seconds) {
    timeval tv{};
    tv.tv_sec = seconds;
    tv.tv_usec = 0;

    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        std::cerr << "[net] Warning: failed to set send timeout on " << GetRemoteAddress()
                  << std::endl;
    }
}

Peer::Peer(Peer&& other) noexcept
    : sockfd(other.sockfd),
      remoteIP(std::move(other.remoteIP)),
      remotePort(other.remotePort),
      connected(other.connected) {
    other.sockfd = -1;
    other.connected = false;
}

Peer& Peer::operator=(Peer&& other) noexcept {
    if (this != &other) {
        Disconnect();
        sockfd = other.sockfd;
        remoteIP = std::move(other.remoteIP);
        remotePort = other.remotePort;
        connected = other.connected;
        other.sockfd = -1;
        other.connected = false;
    }
    return *this;
}

std::vector<uint8_t> Peer::ReadExact(size_t count) {
    std::vector<uint8_t> buffer(count);
    size_t totalRead = 0;

    while (totalRead < count) {
        ssize_t n = recv(sockfd, buffer.data() + totalRead, count - totalRead, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                connected = false;
                throw std::runtime_error("Recv timeout from " + GetRemoteAddress());
            }
            connected = false;
            throw std::runtime_error("Recv error from " + GetRemoteAddress());
        }
        if (n == 0) {
            connected = false;
            throw std::runtime_error("Connection closed by " + GetRemoteAddress());
        }
        totalRead += static_cast<size_t>(n);
    }

    return buffer;
}

void Peer::WriteAll(const std::vector<uint8_t>& data) {
    size_t totalSent = 0;

    while (totalSent < data.size()) {
        ssize_t n = send(sockfd, data.data() + totalSent, data.size() - totalSent, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                connected = false;
                throw std::runtime_error("Send timeout to " + GetRemoteAddress());
            }
            connected = false;
            throw std::runtime_error("Failed to send to " + GetRemoteAddress());
        }
        totalSent += static_cast<size_t>(n);
    }
}

void Peer::SendMessage(const Message& msg) {
    if (!connected) {
        throw std::runtime_error("Not connected to " + GetRemoteAddress());
    }

    std::vector<uint8_t> serialized = msg.Serialize();
    WriteAll(serialized);

    std::cout << "[net] Sent " << msg.GetCommandString() << " to " << GetRemoteAddress() << " ("
              << serialized.size() << " bytes)" << std::endl;
}

Message Peer::ReceiveMessage() {
    if (!connected) {
        throw std::runtime_error("Not connected to " + GetRemoteAddress());
    }

    // read the fixed-size header (24 bytes)
    std::vector<uint8_t> headerData = ReadExact(MESSAGE_HEADER_SIZE);

    // deserialize header
    Message headerOnly = Message::DeserializeHeader(headerData);
    uint32_t payloadLength = headerOnly.GetPayloadLength();

    if (payloadLength > MAX_PAYLOAD_SIZE) {
        throw std::runtime_error("Payload too large (" + std::to_string(payloadLength) +
                                 " bytes) from " + GetRemoteAddress());
    }

    // if there's a payload, read it and deserialize the full message
    if (payloadLength > 0) {
        std::vector<uint8_t> payloadData = ReadExact(payloadLength);

        // combine header and payload
        std::vector<uint8_t> fullMessage;
        fullMessage.reserve(MESSAGE_HEADER_SIZE + payloadLength);
        fullMessage.insert(fullMessage.end(), headerData.begin(), headerData.end());
        fullMessage.insert(fullMessage.end(), payloadData.begin(), payloadData.end());

        Message msg = Message::Deserialize(fullMessage);

        std::cout << "[net] Received " << msg.GetCommandString() << " from " << GetRemoteAddress()
                  << std::endl;

        return msg;
    }

    // verify checksum for empty payload messages
    std::array<uint8_t, CHECKSUM_LENGTH> emptyChecksum = CalculateChecksum({});
    if (headerOnly.GetChecksum() != emptyChecksum) {
        throw std::runtime_error("Checksum verification failed for empty payload from " +
                                 GetRemoteAddress());
    }

    std::cout << "[net] Received " << headerOnly.GetCommandString() << " from "
              << GetRemoteAddress() << std::endl;

    return headerOnly;
}

void Peer::Disconnect() {
    if (sockfd >= 0) {
        close(sockfd);
        sockfd = -1;
    }
    connected = false;
}

std::string Peer::GetRemoteAddress() const { return remoteIP + ":" + std::to_string(remotePort); }

std::unique_ptr<Peer> ConnectToPeer(const std::string& ip, uint16_t port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) <= 0) {
        close(sockfd);
        throw std::runtime_error("Invalid IP address: " + ip);
    }

    if (connect(sockfd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        close(sockfd);
        throw std::runtime_error("Failed to connect to " + ip + ":" + std::to_string(port));
    }

    std::cout << "[net] Connected to " << ip << ":" << port << std::endl;

    return std::make_unique<Peer>(sockfd, ip, port);
}