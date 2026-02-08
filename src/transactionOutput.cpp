#include "transactionOutput.h"

#include "base58.h"
#include "wallet.h"

TransactionOutput::TransactionOutput(int value, const std::vector<uint8_t>& pubKeyHash)
    : value(value), pubKeyHash(pubKeyHash) {}

void TransactionOutput::Lock(const std::vector<uint8_t>& address) {
    std::vector<uint8_t> decoded = Base58Decode(address);
    // extract pubKeyHash: skip version (1 byte) and checksum (last 4 bytes)
    pubKeyHash = std::vector<uint8_t>(decoded.begin() + 1, decoded.end() - ADDRESS_CHECKSUM_LEN);
}

bool TransactionOutput::IsLockedWithKey(const std::vector<uint8_t>& pubKeyHash) const {
    return this->pubKeyHash == pubKeyHash;
}

std::vector<uint8_t> TransactionOutput::Serialize() const {
    std::vector<uint8_t> result;

    // value (4 bytes)
    for (int i = 0; i < 4; i++) {
        result.push_back((value >> (8 * i)) & 0xFF);
    }

    // pubKeyHash size (4 bytes)
    uint32_t pubKeyHashSize = pubKeyHash.size();
    for (int i = 0; i < 4; i++) {
        result.push_back((pubKeyHashSize >> (8 * i)) & 0xFF);
    }

    // pubKeyHash (variable bytes)
    result.insert(result.end(), pubKeyHash.begin(), pubKeyHash.end());

    return result;
}

std::pair<TransactionOutput, size_t> TransactionOutput::Deserialize(
    const std::vector<uint8_t>& data, size_t offset) {
    TransactionOutput output;
    size_t startOffset = offset;

    // value (4 bytes)
    output.value = 0;
    for (int i = 0; i < 4; i++) {
        output.value |= (data[offset + i] << (8 * i));
    }
    offset += 4;

    // pubKeyHash size (4 bytes)
    uint32_t pubKeyHashSize = 0;
    for (int i = 0; i < 4; i++) {
        pubKeyHashSize |= (data[offset + i] << (8 * i));
    }
    offset += 4;

    // pubKeyHash (variable bytes)
    output.pubKeyHash =
        std::vector<uint8_t>(data.begin() + offset, data.begin() + offset + pubKeyHashSize);
    offset += pubKeyHashSize;

    size_t bytesRead = offset - startOffset;
    return {output, bytesRead};
}