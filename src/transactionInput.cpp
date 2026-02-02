#include "transactionInput.h"

TransactionInput::TransactionInput(const std::vector<uint8_t>& txid, int vout,
                                   const std::string& scriptSig)
    : txid(txid), vout(vout), scriptSig(scriptSig) {}

bool TransactionInput::CanUnlockOutputWith(const std::string& unlockingData) const {
    return scriptSig == unlockingData;
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

    // scriptSig size (4 bytes)
    uint32_t scriptSigSize = scriptSig.size();
    for (int i = 0; i < 4; i++) {
        result.push_back((scriptSigSize >> (8 * i)) & 0xFF);
    }

    // scriptSig
    result.insert(result.end(), scriptSig.begin(), scriptSig.end());

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

    // scriptSig size (4 bytes)
    uint32_t scriptSigSize = 0;
    for (int i = 0; i < 4; i++) {
        scriptSigSize |= (data[offset + i] << (8 * i));
    }
    offset += 4;

    // scriptSig
    input.scriptSig = std::string(data.begin() + offset, data.begin() + offset + scriptSigSize);
    offset += scriptSigSize;

    size_t bytesRead = offset - startOffset;
    return {input, bytesRead};
}