#include "messageGetBlocks.h"

#include <stdexcept>

MessageGetBlocks::MessageGetBlocks(const std::vector<uint8_t>& tipHash) : tipHash(tipHash) {
    if (tipHash.size() != 32) {
        throw std::runtime_error("Invalid tip hash size: expected 32 bytes");
    }
}

std::vector<uint8_t> MessageGetBlocks::Serialize() const { return tipHash; }

MessageGetBlocks MessageGetBlocks::Deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 32) {
        throw std::runtime_error("MessageGetBlocks data too small: need 32 bytes");
    }

    std::vector<uint8_t> hash(data.begin(), data.begin() + 32);
    return MessageGetBlocks(hash);
}