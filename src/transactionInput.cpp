#include "transactionInput.h"

#include "utils.h"
#include "wallet.h"

TransactionInput::TransactionInput(const std::vector<uint8_t>& txid, int vout,
                                   const std::vector<uint8_t>& signature,
                                   const std::vector<uint8_t>& pubKey)
    : txid(txid), vout(vout), signature(signature), pubKey(pubKey) {}

bool TransactionInput::UsesKey(const std::vector<uint8_t>& pubKeyHash) const {
    std::vector<uint8_t> lockingHash = Wallet::HashPubKey(pubKey);
    return lockingHash == pubKeyHash;
}

std::vector<uint8_t> TransactionInput::Serialize() const {
    std::vector<uint8_t> result;

    // txid size (4 bytes)
    WriteUint32(result, static_cast<uint32_t>(txid.size()));

    // txid
    result.insert(result.end(), txid.begin(), txid.end());

    // vout (4 bytes)
    WriteUint32(result, static_cast<uint32_t>(vout));

    // signature size (4 bytes)
    WriteUint32(result, static_cast<uint32_t>(signature.size()));

    // signature (variable bytes)
    result.insert(result.end(), signature.begin(), signature.end());

    // pubKey size (4 bytes)
    WriteUint32(result, static_cast<uint32_t>(pubKey.size()));

    // pubKey (variable bytes)
    result.insert(result.end(), pubKey.begin(), pubKey.end());

    return result;
}

std::pair<TransactionInput, size_t> TransactionInput::Deserialize(const std::vector<uint8_t>& data,
                                                                  size_t offset) {
    TransactionInput input;
    size_t startOffset = offset;

    // txid size (4 bytes)
    uint32_t txidSize = ReadUint32(data, offset);
    offset += 4;

    // txid
    if (offset + txidSize > data.size()) {
        throw std::runtime_error("TransactionInput data truncated at txid");
    }
    input.txid = std::vector<uint8_t>(data.begin() + offset, data.begin() + offset + txidSize);
    offset += txidSize;

    // vout (4 bytes)
    input.vout = static_cast<int>(ReadUint32(data, offset));
    offset += 4;

    // signature size (4 bytes)
    uint32_t signatureSize = ReadUint32(data, offset);
    offset += 4;

    // signature (variable bytes)
    if (offset + signatureSize > data.size()) {
        throw std::runtime_error("TransactionInput data truncated at signature");
    }
    input.signature =
        std::vector<uint8_t>(data.begin() + offset, data.begin() + offset + signatureSize);
    offset += signatureSize;

    // pubKey size (4 bytes)
    uint32_t pubKeySize = ReadUint32(data, offset);
    offset += 4;

    // pubKey (variable bytes)
    if (offset + pubKeySize > data.size()) {
        throw std::runtime_error("TransactionInput data truncated at pubKey");
    }
    input.pubKey = std::vector<uint8_t>(data.begin() + offset, data.begin() + offset + pubKeySize);
    offset += pubKeySize;

    size_t bytesRead = offset - startOffset;
    return {input, bytesRead};
}