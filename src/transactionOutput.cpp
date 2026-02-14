#include "transactionOutput.h"

#include "base58.h"
#include "utils.h"
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
    WriteUint32(result, static_cast<uint32_t>(value));

    // pubKeyHash size (4 bytes)
    WriteUint32(result, static_cast<uint32_t>(pubKeyHash.size()));

    // pubKeyHash (variable bytes)
    result.insert(result.end(), pubKeyHash.begin(), pubKeyHash.end());

    return result;
}

std::pair<TransactionOutput, size_t> TransactionOutput::Deserialize(
    const std::vector<uint8_t>& data, size_t offset) {
    TransactionOutput output;
    size_t startOffset = offset;

    // value (4 bytes)
    output.value = static_cast<int>(ReadUint32(data, offset));
    offset += 4;

    // pubKeyHash size (4 bytes)
    uint32_t pubKeyHashSize = ReadUint32(data, offset);
    offset += 4;

    // pubKeyHash (variable bytes)
    if (offset + pubKeyHashSize > data.size()) {
        throw std::runtime_error("TransactionOutput data truncated at pubKeyHash");
    }
    output.pubKeyHash =
        std::vector<uint8_t>(data.begin() + offset, data.begin() + offset + pubKeyHashSize);
    offset += pubKeyHashSize;

    size_t bytesRead = offset - startOffset;
    return {output, bytesRead};
}

// factory function to create a new transaction output
TransactionOutput NewTXOutput(int value, const std::string& address) {
    TransactionOutput txo(value, {});
    txo.Lock(StringToBytes(address));
    return txo;
}

std::vector<uint8_t> TXOutputs::Serialize() const {
    std::vector<uint8_t> result;

    // number of outputs (4 bytes)
    WriteUint32(result, static_cast<uint32_t>(outputs.size()));

    // each output
    for (const auto& output : outputs) {
        std::vector<uint8_t> outputSerialized = output.Serialize();
        result.insert(result.end(), outputSerialized.begin(), outputSerialized.end());
    }

    return result;
}

TXOutputs TXOutputs::Deserialize(const std::vector<uint8_t>& data) {
    TXOutputs txOutputs;
    size_t offset = 0;

    if (data.size() < 4) {
        throw std::runtime_error("TXOutputs data too small to deserialize");
    }

    // number of outputs (4 bytes)
    uint32_t outputCount = ReadUint32(data, offset);
    offset += 4;

    // each output
    for (uint32_t i = 0; i < outputCount; i++) {
        auto [output, bytesRead] = TransactionOutput::Deserialize(data, offset);
        txOutputs.outputs.push_back(output);
        offset += bytesRead;
    }

    return txOutputs;
}