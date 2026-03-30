#ifndef UTXOSET_H
#define UTXOSET_H

#include <leveldb/db.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "transactionOutput.h"

class Blockchain;
class Block;
class Transaction;

// this is a persistent UTXO set backed by its own LevelDB instance under data/utxo/
class UTXOSet {
        friend class Transaction;

    private:
        Blockchain* blockchain;
        std::unique_ptr<leveldb::DB> db;

    public:
        explicit UTXOSet(Blockchain* bc);
        ~UTXOSet() = default;

        // prevent copying
        UTXOSet(const UTXOSet&) = delete;
        UTXOSet& operator=(const UTXOSet&) = delete;

        std::pair<int64_t, std::map<std::string, std::vector<int>>> FindSpendableOutputs(
            const std::vector<uint8_t>& pubKeyHash, int64_t amount) const;

        std::vector<TransactionOutput> FindUTXO(const std::vector<uint8_t>& pubKeyHash) const;

        int CountTransactions() const;

        void Reindex();

        void Update(const Block& block);
};

#endif