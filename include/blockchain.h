#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <leveldb/db.h>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "block.h"
#include "blockchainIterator.h"
#include "config.h"
#include "transaction.h"

// Forward declaration
class Wallet;
class UTXOSet;

class Blockchain {
        friend class UTXOSet;

    private:
        std::vector<uint8_t> tip;         // hash of the last block
        int32_t tipHeight{0};             // height of tip, cached in memory and persisted to DB
        std::unique_ptr<leveldb::DB> db;  // leveldb for storing blocks (persistency)

    public:
        Blockchain();
        ~Blockchain() = default;

        // prevent copying
        Blockchain(const Blockchain&) = delete;
        Blockchain& operator=(const Blockchain&) = delete;

        Block MineBlock(const std::vector<Transaction>& transactions);

        // adds a mined block received from a peer.
        void AddBlock(const Block& block);
        Block GetBlock(const std::vector<uint8_t>& hash) const;
        std::vector<std::vector<uint8_t>> GetBlockHashesAfter(
            const std::vector<uint8_t>& afterHash) const;

        const std::vector<uint8_t>& GetTip() const { return tip; }

        // zero based height of the chain (genesis = 0)
        int32_t GetChainHeight() const;

        // height of any block by hash and -1 if not found
        int32_t GetBlockHeight(const std::vector<uint8_t>& hash) const;

        int32_t GetNextWorkRequired(int32_t nextBlockHeight) const;

        std::map<std::string, TXOutputs> FindUTXO();

        Transaction FindTransaction(const std::vector<uint8_t>& ID);
        void SignTransaction(Transaction* tx, Wallet* wallet);

        // returns the fee on success, nullopt on failure
        std::optional<int64_t> VerifyTransaction(const Transaction* tx);

        // overload that accepts an intra-block context for topological verification
        std::optional<int64_t> VerifyTransaction(
            const Transaction* tx, const std::map<std::string, Transaction>& blockCtx);

        BlockchainIterator Iterator() const;

        static bool DBExists();
        static std::unique_ptr<Blockchain> CreateBlockchain(const std::string& address);
};

#endif