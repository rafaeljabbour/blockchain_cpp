#ifndef BLOCK_H
#define BLOCK_H

#include <cstdint>
#include <string>
#include <vector>

#include "config.h"
#include "transaction.h"

class Block {
    private:
        int64_t timestamp;  // when block is created
        std::vector<Transaction> transactions;
        std::vector<uint8_t> previousHash;  // Stores hash of previous block
        std::vector<uint8_t> hash;
        int32_t nonce;  // counter to try different hashes in pow
        int32_t bits;   // difficulty target for this block

    public:
        Block(const std::vector<Transaction>& transactions,
              const std::vector<uint8_t>& previousHash, int32_t bits);
        Block() = default;

        int64_t GetTimestamp() const { return timestamp; }
        const std::vector<Transaction>& GetTransactions() const { return transactions; }
        const std::vector<uint8_t>& GetPreviousHash() const { return previousHash; }
        const std::vector<uint8_t>& GetHash() const { return hash; }
        int32_t GetNonce() const { return nonce; }
        int32_t GetBits() const { return bits; }

        static Block NewGenesisBlock(const Transaction& coinbase);

        static bool CheckBlockSize(const Block& block, size_t knownSerializedSize = 0);

        std::vector<uint8_t> Serialize() const;
        static Block Deserialize(const std::vector<uint8_t>& serialized);
        std::vector<uint8_t> HashTransactions() const;
};

#endif