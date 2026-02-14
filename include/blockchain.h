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

// Forward declaration
class Wallet;
class UTXOSet;

inline const std::string DB_FILE = "./data/blocks";
inline const std::string GENESIS_COINBASE_DATA =
    "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";

class Blockchain {
        friend class UTXOSet;

    private:
        std::vector<uint8_t> tip;         // hash of the last block
        std::unique_ptr<leveldb::DB> db;  // leveldb for storing blocks (persistency)

    public:
        Blockchain();
        ~Blockchain() = default;

        // prevent copying
        Blockchain(const Blockchain&) = delete;
        Blockchain& operator=(const Blockchain&) = delete;

        Block MineBlock(const std::vector<Transaction>& transactions);

        std::map<std::string, TXOutputs> FindUTXO();

        Transaction FindTransaction(const std::vector<uint8_t>& ID);
        void SignTransaction(Transaction* tx, Wallet* wallet);
        bool VerifyTransaction(const Transaction* tx);

        BlockchainIterator Iterator();

        static bool DBExists();
        static std::unique_ptr<Blockchain> CreateBlockchain(const std::string& address);
};

#endif