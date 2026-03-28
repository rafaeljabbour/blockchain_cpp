#ifndef MESSAGEADDR_H
#define MESSAGEADDR_H

#include <cstdint>
#include <vector>

#include "message.h"
#include "netAddr.h"

// the max number of addresses received from a peer
inline constexpr uint32_t MAX_ADDR_PER_MSG = 1000;

// Carries a list of known peer addresses with timestamps.
class MessageAddr {
    private:
        std::vector<NetAddr> addresses;

    public:
        MessageAddr() = default;
        explicit MessageAddr(const std::vector<NetAddr>& addresses);

        const std::vector<NetAddr>& GetAddresses() const { return addresses; }
        uint32_t GetCount() const { return static_cast<uint32_t>(addresses.size()); }

        std::vector<uint8_t> Serialize() const;
        static MessageAddr Deserialize(const std::vector<uint8_t>& data);
};

inline Message CreateGetAddrMessage() { return Message(MAGIC_CUSTOM, CMD_GETADDR, {}); }

#endif