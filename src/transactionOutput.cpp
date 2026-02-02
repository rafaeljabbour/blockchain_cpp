#include "transactionOutput.h"

TransactionOutput::TransactionOutput(int value, const std::string& scriptPubKey)
    : value(value), scriptPubKey(scriptPubKey) {}

bool TransactionOutput::CanBeUnlockedWith(const std::string& unlockingData) const {
    return scriptPubKey == unlockingData;
}

std::vector<uint8_t> TransactionOutput::Serialize() const {
    std::vector<uint8_t> result;

    // value (4 bytes)
    for (int i = 0; i < 4; i++) {
        result.push_back((value >> (8 * i)) & 0xFF);
    }

    // scriptPubKey size (4 bytes)
    uint32_t scriptPubKeySize = scriptPubKey.size();
    for (int i = 0; i < 4; i++) {
        result.push_back((scriptPubKeySize >> (8 * i)) & 0xFF);
    }

    // scriptPubKey
    result.insert(result.end(), scriptPubKey.begin(), scriptPubKey.end());

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

    // scriptPubKey size (4 bytes)
    uint32_t scriptPubKeySize = 0;
    for (int i = 0; i < 4; i++) {
        scriptPubKeySize |= (data[offset + i] << (8 * i));
    }
    offset += 4;

    // scriptPubKey
    output.scriptPubKey =
        std::string(data.begin() + offset, data.begin() + offset + scriptPubKeySize);
    offset += scriptPubKeySize;

    size_t bytesRead = offset - startOffset;
    return {output, bytesRead};
}