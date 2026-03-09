#include "transactionOutput.h"

#include <stdexcept>

#include "base58.h"
#include "serialization.h"
#include "wallet.h"

TransactionOutput::TransactionOutput(int64_t value, const std::vector<uint8_t>& pubKeyHash)
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

    // value (8 bytes)
    WriteUint64(result, static_cast<uint64_t>(value));

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

    // value (8 bytes)
    output.value = static_cast<int64_t>(ReadUint64(data, offset));
    offset += 8;

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
TransactionOutput NewTXOutput(int64_t value, const std::string& address) {
    TransactionOutput txo(value, {});
    txo.Lock(StringToBytes(address));
    return txo;
}

std::vector<uint8_t> TXOutputs::Serialize() const {
    std::vector<uint8_t> result;

    // coinbase flag (1 byte)
    result.push_back(isCoinbase ? 0x01 : 0x00);

    // block height this transaction was confirmed at (4 bytes)
    WriteUint32(result, static_cast<uint32_t>(blockHeight));

    // number of outputs (4 bytes)
    WriteUint32(result, static_cast<uint32_t>(outputs.size()));

    // each (original index, output) pair
    for (const auto& [origIdx, output] : outputs) {
        // original output index (4 bytes)
        WriteUint32(result, static_cast<uint32_t>(origIdx));

        std::vector<uint8_t> outputSerialized = output.Serialize();
        result.insert(result.end(), outputSerialized.begin(), outputSerialized.end());
    }

    return result;
}

TXOutputs TXOutputs::Deserialize(const std::vector<uint8_t>& data) {
    TXOutputs txOutputs;
    size_t offset = 0;

    // isCoinbase(1) + blockHeight(4) + outputCount(4) = 9
    if (data.size() < 9) {
        throw std::runtime_error("TXOutputs data too small to deserialize");
    }

    // coinbase flag (1 byte)
    txOutputs.isCoinbase = (data[offset] == 0x01);
    offset += 1;

    // block height (4 bytes)
    txOutputs.blockHeight = static_cast<int32_t>(ReadUint32(data, offset));
    offset += 4;

    // number of outputs (4 bytes)
    uint32_t outputCount = ReadUint32(data, offset);
    offset += 4;

    // each (original index, output) pair
    for (uint32_t i = 0; i < outputCount; i++) {
        // original output index (4 bytes)
        if (offset + 4 > data.size()) {
            throw std::runtime_error("TXOutputs data truncated at output index");
        }
        int origIdx = static_cast<int>(ReadUint32(data, offset));
        offset += 4;

        auto [output, bytesRead] = TransactionOutput::Deserialize(data, offset);
        txOutputs.outputs[origIdx] = output;
        offset += bytesRead;
    }

    return txOutputs;
}