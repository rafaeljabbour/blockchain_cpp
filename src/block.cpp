#include "block.h"

#include <cstdint>
#include <ctime>
#include <vector>

#include "proofOfWork.h"
#include "utils.h"

Block::Block(const std::vector<Transaction>& transactions,
             const std::vector<uint8_t>& previousHash) {
    timestamp = std::time(nullptr);
    this->transactions = transactions;
    this->previousHash = previousHash;

    ProofOfWork proofOfWork(this);
    std::pair<int32_t, std::vector<uint8_t>> powResult = proofOfWork.Run();
    nonce = powResult.first;
    hash = powResult.second;
}

std::vector<uint8_t> Block::Serialize() const {
    std::vector<uint8_t> serialized;

    // timestamp (8 bytes)
    for (int i = 0; i < 8; i++) {
        serialized.push_back((timestamp >> (8 * i)) & 0xFF);
    }

    // number of transaction (4 bytes)
    uint32_t txCount = transactions.size();
    for (int i = 0; i < 4; i++) {
        serialized.push_back((txCount >> (8 * i)) & 0xFF);
    }

    // each transaction
    for (const Transaction& tx : transactions) {
        std::vector<uint8_t> txSerialized = tx.Serialize();
        uint32_t txSize = txSerialized.size();

        // transaction size (4 bytes)
        for (int i = 0; i < 4; i++) {
            serialized.push_back((txSize >> (8 * i)) & 0xFF);
        }

        // transaction (variable bytes)
        serialized.insert(serialized.end(), txSerialized.begin(), txSerialized.end());
    }

    // previous hash (32 bytes)
    if (!previousHash.empty()) {
        serialized.insert(serialized.end(), previousHash.begin(), previousHash.end());
    } else {
        serialized.insert(serialized.end(), 32, 0);
    }

    // hash (32 bytes)
    serialized.insert(serialized.end(), hash.begin(), hash.end());

    // nonce (4 bytes)
    for (int i = 0; i < 4; i++) {
        serialized.push_back((nonce >> (8 * i)) & 0xFF);
    }

    return serialized;
}

Block Block::Deserialize(const std::vector<uint8_t>& serialized) {
    Block block;
    size_t offset = 0;

    // timestamp (8 bytes)
    block.timestamp = 0;
    for (int i = 0; i < 8; i++) {
        block.timestamp |= (serialized[offset + i] << (8 * i));
    }

    offset += 8;
    // number of transactions (4 bytes)
    uint32_t txCount = 0;
    for (int i = 0; i < 4; i++) {
        txCount |= (serialized[offset + i] << (i * 8));
    }

    offset += 4;
    // each transaction
    for (uint32_t i = 0; i < txCount; i++) {
        // transaction size (4 bytes)
        uint32_t txSize = 0;
        for (int i = 0; i < 4; i++) {
            txSize |= (serialized[offset + i] << (i * 8));
        }
        offset += 4;

        // transaction (variable bytes)
        block.transactions.push_back(Transaction::Deserialize(std::vector<uint8_t>(
            serialized.begin() + offset, serialized.begin() + offset + txSize)));
        offset += txSize;
    }

    // previous hash (32 bytes)
    block.previousHash =
        std::vector<uint8_t>(serialized.begin() + offset, serialized.begin() + offset + 32);

    offset += 32;
    // hash (32 bytes)
    block.hash =
        std::vector<uint8_t>(serialized.begin() + offset, serialized.begin() + offset + 32);

    offset += 32;
    // nonce (4 bytes)
    block.nonce = 0;
    for (int i = 0; i < 4; i++) {
        block.nonce |= (static_cast<int32_t>(serialized[offset + i] << (8 * i)));
    }

    return block;
}

std::vector<uint8_t> Block::HashTransactions() const {
    std::vector<uint8_t> txHashes;

    for (const Transaction& tx : transactions) {
        std::vector<uint8_t> txHash = tx.Hash();
        txHashes.insert(txHashes.end(), txHash.begin(), txHash.end());
    }

    return SHA256Hash(txHashes);
}

Block Block::NewGenesisBlock(const Transaction& coinbase) {
    std::vector<Transaction> transactions = {coinbase};
    return Block(transactions, std::vector<uint8_t>(32, 0));
}