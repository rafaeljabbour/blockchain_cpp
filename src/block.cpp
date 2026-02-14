#include "block.h"

#include <cstdint>
#include <ctime>
#include <vector>

#include "merkleTree.h"
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
    WriteUint64(serialized, static_cast<uint64_t>(timestamp));

    // number of transactions (4 bytes)
    WriteUint32(serialized, static_cast<uint32_t>(transactions.size()));

    // each transaction
    for (const Transaction& tx : transactions) {
        std::vector<uint8_t> txSerialized = tx.Serialize();

        // transaction size (4 bytes)
        WriteUint32(serialized, static_cast<uint32_t>(txSerialized.size()));

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
    WriteUint32(serialized, static_cast<uint32_t>(nonce));

    return serialized;
}

Block Block::Deserialize(const std::vector<uint8_t>& serialized) {
    Block block;
    size_t offset = 0;

    if (serialized.size() < 8 + 4 + 32 + 32 + 4) {
        throw std::runtime_error("Block data too small to deserialize");
    }

    // timestamp (8 bytes)
    block.timestamp = static_cast<int64_t>(ReadUint64(serialized, offset));
    offset += 8;

    // number of transactions (4 bytes)
    uint32_t txCount = ReadUint32(serialized, offset);
    offset += 4;

    // each transaction
    for (uint32_t i = 0; i < txCount; i++) {
        // transaction size (4 bytes)
        uint32_t txSize = ReadUint32(serialized, offset);
        offset += 4;

        if (offset + txSize > serialized.size()) {
            throw std::runtime_error("Block data truncated: transaction extends past end");
        }

        // transaction (variable bytes)
        block.transactions.push_back(Transaction::Deserialize(std::vector<uint8_t>(
            serialized.begin() + offset, serialized.begin() + offset + txSize)));
        offset += txSize;
    }

    if (offset + 32 + 32 + 4 > serialized.size()) {
        throw std::runtime_error("Block data truncated: missing hash or nonce");
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
    block.nonce = static_cast<int32_t>(ReadUint32(serialized, offset));

    return block;
}

std::vector<uint8_t> Block::HashTransactions() const {
    MerkleTree tree(transactions);
    return tree.GetRootHash();
}

Block Block::NewGenesisBlock(const Transaction& coinbase) {
    std::vector<Transaction> transactions = {coinbase};
    return Block(transactions, std::vector<uint8_t>(32, 0));
}