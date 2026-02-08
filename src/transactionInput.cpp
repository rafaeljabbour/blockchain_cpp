#include "transactionInput.h"

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
    uint32_t txidSize = txid.size();
    for (int i = 0; i < 4; i++) {
        result.push_back((txidSize >> (8 * i)) & 0xFF);
    }

    // txid
    result.insert(result.end(), txid.begin(), txid.end());

    // vout (4 bytes)
    for (int i = 0; i < 4; i++) {
        result.push_back((vout >> (8 * i)) & 0xFF);
    }
    // signature size (4 bytes)
    uint32_t signatureSize = signature.size();
    for (int i = 0; i < 4; i++) {
        result.push_back((signatureSize >> (8 * i)) & 0xFF);
    }

    // signature (variable bytes)
    result.insert(result.end(), signature.begin(), signature.end());

    // pubKey size (4 bytes)
    uint32_t pubKeySize = pubKey.size();
    for (int i = 0; i < 4; i++) {
        result.push_back((pubKeySize >> (8 * i)) & 0xFF);
    }

    // pubKey (variable bytes)
    result.insert(result.end(), pubKey.begin(), pubKey.end());

    return result;
}

std::pair<TransactionInput, size_t> TransactionInput::Deserialize(const std::vector<uint8_t>& data,
                                                                  size_t offset) {
    TransactionInput input;
    size_t startOffset = offset;

    // txid size (4 bytes)
    uint32_t txidSize = 0;
    for (int i = 0; i < 4; i++) {
        txidSize |= (data[offset + i] << (8 * i));
    }
    offset += 4;

    // txid
    input.txid = std::vector<uint8_t>(data.begin() + offset, data.begin() + offset + txidSize);
    offset += txidSize;

    // vout (4 bytes)
    input.vout = 0;
    for (int i = 0; i < 4; i++) {
        input.vout |= (data[offset + i] << (8 * i));
    }
    offset += 4;

    // signature size (4 bytes)
    uint32_t signatureSize = 0;
    for (int i = 0; i < 4; i++) {
        signatureSize |= (data[offset + i] << (8 * i));
    }
    offset += 4;

    // signature (variable bytes)
    input.signature =
        std::vector<uint8_t>(data.begin() + offset, data.begin() + offset + signatureSize);
    offset += signatureSize;

    // pubKey size (4 bytes)
    uint32_t pubKeySize = 0;
    for (int i = 0; i < 4; i++) {
        pubKeySize |= (data[offset + i] << (8 * i));
    }
    offset += 4;

    // pubKey (variable bytes)
    input.pubKey = std::vector<uint8_t>(data.begin() + offset, data.begin() + offset + pubKeySize);
    offset += pubKeySize;

    size_t bytesRead = offset - startOffset;
    return {input, bytesRead};
}