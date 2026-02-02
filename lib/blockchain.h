#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <leveldb/db.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "block.h"
#include "blockchainIterator.h"
#include "transaction.h"

inline const std::string DB_FILE = "./tmp/blocks";
inline const std::string BLOCKS_BUCKET = "blocks";
inline const std::string GENESIS_COINBASE_DATA =
    "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";

class Blockchain {
    private:
        std::vector<uint8_t> tip;  // hash of the last block
        leveldb::DB* db;           // leveldb for storing blocks (persistency)

    public:
        Blockchain();
        ~Blockchain();

        void MineBlock(const std::vector<Transaction>& transactions);

        std::vector<Transaction> FindUnspentTransactions(const std::string& address);
        std::vector<TransactionOutput> FindUTXO(const std::string& address);
        std::pair<int, std::map<std::string, std::vector<int>>> FindSpendableOutputs(
            const std::string& address, int amount);

        BlockchainIterator Iterator();

        leveldb::DB* GetDB() const { return db; }
        const std::vector<uint8_t>& GetTip() const { return tip; }

        static bool DBExists();
        static std::unique_ptr<Blockchain> CreateBlockchain(const std::string& address);
};

#endif