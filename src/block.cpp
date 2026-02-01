#include "block.h"

#include <openssl/sha.h>

#include <ctime>

#include "proofOfWork.h"

Block::Block(const std::string& data,
             const std::vector<uint8_t>& previousHash) {
    timestamp = std::time(nullptr);
    this->data = std::vector<uint8_t>(data.begin(), data.end());
    this->previousHash = previousHash;

    ProofOfWork proofOfWork(this);
    std::pair<int32_t, std::vector<uint8_t>> powResult = proofOfWork.Run();
    nonce = powResult.first;
    hash = powResult.second;
}

std::vector<uint8_t> Block::Serialize() const {
    std::vector<uint8_t> serialized;

    for (int i = 0; i < 8; i++) {
        serialized.push_back((timestamp >> (8 * i)) & 0xFF);
    }

    uint32_t dataSize = data.size();
    for (int i = 0; i < 4; i++) {
        serialized.push_back((dataSize >> (8 * i)) & 0xFF);
    }
    serialized.insert(serialized.end(), data.begin(), data.end());

    if (!previousHash.empty()) {
        serialized.insert(serialized.end(), previousHash.begin(),
                          previousHash.end());
    } else {
        serialized.insert(serialized.end(), 32, 0);
    }
    serialized.insert(serialized.end(), hash.begin(), hash.end());

    for (int i = 0; i < 4; i++) {
        serialized.push_back((nonce >> (8 * i)) & 0xFF);
    }

    return serialized;
}

Block Block::Deserialize(const std::vector<uint8_t>& serialized) {
    Block block;
    size_t offset = 0;

    block.timestamp = 0;
    for (int i = 0; i < 8; i++) {
        block.timestamp |= (serialized[offset + i] << (8 * i));
    }

    offset += 8;
    uint32_t dataSize = 0;
    for (int i = 0; i < 4; i++) {
        dataSize |= (serialized[offset + i] << (i * 8));
    }

    offset += 4;
    block.data = std::vector<uint8_t>(serialized.begin() + offset,
                                      serialized.begin() + offset + dataSize);

    offset += dataSize;
    block.previousHash = std::vector<uint8_t>(serialized.begin() + offset,
                                              serialized.begin() + offset + 32);

    offset += 32;
    block.hash = std::vector<uint8_t>(serialized.begin() + offset,
                                      serialized.begin() + offset + 32);

    offset += 32;
    block.nonce = 0;
    for (int i = 0; i < 4; i++) {
        block.nonce |=
            (static_cast<int32_t>(serialized[offset + i] << (8 * i)));
    }

    return block;
}