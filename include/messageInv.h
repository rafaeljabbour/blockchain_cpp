#ifndef MESSAGEINV_H
#define MESSAGEINV_H

#include <cstdint>
#include <utility>
#include <vector>

#include "message.h"

// inventory identifiers
enum class InvType : uint32_t {
    Error = 0,
    Tx = 1,
    Block = 2,
};

// inventory vector to identifying an object by type and hash
struct InvVector {
        InvType type;
        std::vector<uint8_t> hash;  // transaction ID or block hash

        std::vector<uint8_t> Serialize() const;
        static std::pair<InvVector, size_t> Deserialize(const std::vector<uint8_t>& data,
                                                        size_t offset);
};

// to announces available transactions or blocks to a peer.
class MessageInv {
    private:
        uint8_t count;
        std::vector<InvVector> inventory;

    public:
        MessageInv() : count(0) {}
        explicit MessageInv(const std::vector<InvVector>& inventory);

        uint8_t GetCount() const { return count; }
        const std::vector<InvVector>& GetInventory() const { return inventory; }

        std::vector<uint8_t> Serialize() const;
        static MessageInv Deserialize(const std::vector<uint8_t>& data);
};

// to request the full data for objects listed in inventory vectors
using MessageGetData = MessageInv;

#endif