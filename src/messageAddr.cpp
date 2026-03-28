#include "messageAddr.h"

#include <stdexcept>

#include "serialization.h"

MessageAddr::MessageAddr(const std::vector<NetAddr>& addresses) : addresses(addresses) {
    if (addresses.size() > MAX_ADDR_PER_MSG) {
        throw std::runtime_error("addr message exceeds maximum of " +
                                 std::to_string(MAX_ADDR_PER_MSG) + " addresses");
    }
}

std::vector<uint8_t> MessageAddr::Serialize() const {
    std::vector<uint8_t> result;

    // address count (4 bytes)
    WriteUint32(result, static_cast<uint32_t>(addresses.size()));

    // each address serialized with time
    for (const auto& addr : addresses) {
        std::vector<uint8_t> addrSerialized = addr.Serialize(/*includeTime=*/true);
        result.insert(result.end(), addrSerialized.begin(), addrSerialized.end());
    }

    return result;
}

MessageAddr MessageAddr::Deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 4) {
        throw std::runtime_error("MessageAddr data too small to deserialize");
    }

    size_t offset = 0;

    // address count (4 bytes)
    uint32_t count = ReadUint32(data, offset);
    offset += 4;

    if (count > MAX_ADDR_PER_MSG) {
        throw std::runtime_error("addr message contains " + std::to_string(count) +
                                 " addresses, exceeding limit of " +
                                 std::to_string(MAX_ADDR_PER_MSG));
    }

    std::vector<NetAddr> addresses;
    addresses.reserve(count);

    for (uint32_t i = 0; i < count; i++) {
        auto [addr, bytesRead] = NetAddr::Deserialize(data, offset, /*includeTime=*/true);
        addresses.push_back(addr);
        offset += bytesRead;
    }

    return MessageAddr(addresses);
}