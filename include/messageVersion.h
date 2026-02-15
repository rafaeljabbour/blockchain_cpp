#ifndef MESSAGEVERSION_H
#define MESSAGEVERSION_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "netAddr.h"

// protocol version of node
const int32_t PROTOCOL_VERSION = 1;

// service flags
const uint64_t NODE_NETWORK = 1;

class MessageVersion {
    private:
        int32_t version;        // protocol version
        uint64_t services;      // services this node provides
        int64_t timestamp;      // current timestamp
        NetAddr addrRecv;       // address of the node receiving this message
        NetAddr addrFrom;       // address of the node sending this message
        uint64_t nonce;         // random number for connection identification
        std::string userAgent;  // software name and version
        int32_t startHeight;    // last block number we have (-1 if no blocks)
        bool relay;             // whether we want to receive transaction broadcasts

    public:
        MessageVersion() = default;
        MessageVersion(const std::string& receiverIP, uint16_t receiverPort,
                       const std::string& senderIP, uint16_t senderPort, int32_t startHeight,
                       bool relay = true);

        int32_t GetVersion() const { return version; }
        uint64_t GetServices() const { return services; }
        int64_t GetTimestamp() const { return timestamp; }
        const NetAddr& GetAddrRecv() const { return addrRecv; }
        const NetAddr& GetAddrFrom() const { return addrFrom; }
        uint64_t GetNonce() const { return nonce; }
        const std::string& GetUserAgent() const { return userAgent; }
        int32_t GetStartHeight() const { return startHeight; }
        bool GetRelay() const { return relay; }

        std::vector<uint8_t> Serialize() const;
        static MessageVersion Deserialize(const std::vector<uint8_t>& data);
};

#endif