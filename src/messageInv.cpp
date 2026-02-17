#include "messageInv.h"

#include <stdexcept>

#include "utils.h"

// for the inventory vector
std::vector<uint8_t> InvVector::Serialize() const {
    std::vector<uint8_t> result;

    // type (4 bytes)
    WriteUint32(result, static_cast<uint32_t>(type));

    // hash size (4 bytes)
    WriteUint32(result, static_cast<uint32_t>(hash.size()));

    // hash (variable bytes)
    result.insert(result.end(), hash.begin(), hash.end());

    return result;
}

std::pair<InvVector, size_t> InvVector::Deserialize(const std::vector<uint8_t>& data,
                                                    size_t offset) {
    InvVector invVec;
    size_t startOffset = offset;

    // type (4 bytes)
    if (offset + 4 > data.size()) {
        throw std::runtime_error("InvVector data truncated at type");
    }
    uint32_t rawType = ReadUint32(data, offset);
    if (rawType > static_cast<uint32_t>(InvType::Block)) {
        throw std::runtime_error("Unknown inventory type: " + std::to_string(rawType));
    }
    invVec.type = static_cast<InvType>(rawType);
    offset += 4;

    // hash size (4 bytes)
    if (offset + 4 > data.size()) {
        throw std::runtime_error("InvVector data truncated at hash size");
    }
    uint32_t hashSize = ReadUint32(data, offset);
    offset += 4;

    // hash (variable bytes)
    if (offset + hashSize > data.size()) {
        throw std::runtime_error("InvVector data truncated at hash");
    }
    invVec.hash = std::vector<uint8_t>(data.begin() + offset, data.begin() + offset + hashSize);
    offset += hashSize;

    size_t bytesRead = offset - startOffset;
    return {invVec, bytesRead};
}

// for the messageInv and the messageGetData
MessageInv::MessageInv(const std::vector<InvVector>& inventory) : inventory(inventory) {
    if (inventory.size() > 255) {
        throw std::runtime_error("Inventory count exceeds uint8_t max (255)");
    }
    count = static_cast<uint8_t>(inventory.size());
}

std::vector<uint8_t> MessageInv::Serialize() const {
    std::vector<uint8_t> result;

    // count (1 byte)
    result.push_back(count);

    // inventory vectors (variable bytes)
    for (const auto& invVec : inventory) {
        std::vector<uint8_t> invVecData = invVec.Serialize();
        result.insert(result.end(), invVecData.begin(), invVecData.end());
    }

    return result;
}

MessageInv MessageInv::Deserialize(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        throw std::runtime_error("MessageInv data too small to deserialize");
    }

    size_t offset = 0;

    // count (1 byte)
    uint8_t count = data[offset];
    offset += 1;

    // inventory vectors
    std::vector<InvVector> inventory;
    inventory.reserve(count);

    for (uint8_t i = 0; i < count; i++) {
        auto [invVec, bytesRead] = InvVector::Deserialize(data, offset);
        inventory.push_back(invVec);
        offset += bytesRead;
    }

    return MessageInv(inventory);
}