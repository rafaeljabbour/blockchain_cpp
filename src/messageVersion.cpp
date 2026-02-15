#include "messageVersion.h"

#include <ctime>
#include <random>
#include <stdexcept>

#include "utils.h"

MessageVersion::MessageVersion(const std::string& receiverIP, uint16_t receiverPort,
                               const std::string& senderIP, uint16_t senderPort,
                               int32_t startHeight, bool relay)
    : version(PROTOCOL_VERSION), services(NODE_NETWORK), startHeight(startHeight), relay(relay) {
    // current timestamp
    timestamp = static_cast<int64_t>(std::time(nullptr));

    // network addresses (time is 0 for version messages)
    addrRecv = NetAddr(NODE_NETWORK, receiverIP, receiverPort);
    addrFrom = NetAddr(NODE_NETWORK, senderIP, senderPort);

    // random nonce for connection identification
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    nonce = dis(gen);

    // user agent identifying our software
    userAgent = "/CustomBlockchain:0.0.1/";
}

std::vector<uint8_t> MessageVersion::Serialize() const {
    std::vector<uint8_t> result;

    // version (4 bytes)
    WriteUint32(result, static_cast<uint32_t>(version));

    // services (8 bytes)
    WriteUint64(result, services);

    // timestamp (8 bytes)
    WriteUint64(result, static_cast<uint64_t>(timestamp));

    // addrRecv (26 bytes)
    std::vector<uint8_t> recvSerialized = addrRecv.Serialize(false);
    result.insert(result.end(), recvSerialized.begin(), recvSerialized.end());

    // addrFrom (26 bytes)
    std::vector<uint8_t> fromSerialized = addrFrom.Serialize(false);
    result.insert(result.end(), fromSerialized.begin(), fromSerialized.end());

    // nonce (8 bytes)
    WriteUint64(result, nonce);

    // userAgent (variable bytes)
    if (userAgent.length() > 255) {
        throw std::runtime_error("User agent too long (max 255 characters)");
    }
    result.push_back(static_cast<uint8_t>(userAgent.length()));
    result.insert(result.end(), userAgent.begin(), userAgent.end());

    // startHeight (4 bytes)
    WriteUint32(result, static_cast<uint32_t>(startHeight));

    // relay (1 byte)
    result.push_back(relay ? 0x01 : 0x00);

    return result;
}

MessageVersion MessageVersion::Deserialize(const std::vector<uint8_t>& data) {
    MessageVersion msg;
    size_t offset = 0;

    // minimum size: 4+8+8+26+26+8+1+4+1 = 86 bytes
    if (data.size() < 86) {
        throw std::runtime_error("MessageVersion data too small to deserialize");
    }

    // version (4 bytes)
    msg.version = static_cast<int32_t>(ReadUint32(data, offset));
    offset += 4;

    // services (8 bytes)
    msg.services = ReadUint64(data, offset);
    offset += 8;

    // timestamp (8 bytes)
    msg.timestamp = static_cast<int64_t>(ReadUint64(data, offset));
    offset += 8;

    // addrRecv (26 bytes)
    auto [addrRecv, recvBytes] = NetAddr::Deserialize(data, offset, false);
    msg.addrRecv = addrRecv;
    offset += recvBytes;

    // addrFrom (26 bytes)
    auto [addrFrom, fromBytes] = NetAddr::Deserialize(data, offset, false);
    msg.addrFrom = addrFrom;
    offset += fromBytes;

    // nonce (8 bytes)
    msg.nonce = ReadUint64(data, offset);
    offset += 8;

    // userAgent (variable bytes)
    if (offset >= data.size()) {
        throw std::runtime_error("MessageVersion data truncated at user agent length");
    }
    uint8_t userAgentLen = data[offset];
    offset += 1;

    if (offset + userAgentLen > data.size()) {
        throw std::runtime_error("MessageVersion data truncated at user agent");
    }
    msg.userAgent = std::string(data.begin() + offset, data.begin() + offset + userAgentLen);
    offset += userAgentLen;

    // startHeight (4 bytes)
    if (offset + 4 > data.size()) {
        throw std::runtime_error("MessageVersion data truncated at start height");
    }
    msg.startHeight = static_cast<int32_t>(ReadUint32(data, offset));
    offset += 4;

    // relay (1 byte)
    if (offset >= data.size()) {
        throw std::runtime_error("MessageVersion data truncated at relay flag");
    }
    msg.relay = data[offset] != 0x00;

    return msg;
}