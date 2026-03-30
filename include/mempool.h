#ifndef MEMPOOL_H
#define MEMPOOL_H

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "block.h"
#include "transaction.h"

struct MempoolEntry {
        Transaction tx;
        double feeRate;  // raffys per serialized byte
        size_t txSize;   // serialized size in bytes
};

// stores unconfirmed transactions.
class Mempool {
    private:
        std::map<std::string, MempoolEntry> entries;
        size_t totalBytes = 0;
        mutable std::mutex mtx;

        // evicts the lowest fee rate entry, returns false if mempool is empty
        bool EvictLowestFeeRate();

    public:
        Mempool() = default;

        bool AddTransaction(const Transaction& tx, double feeRate);
        void RemoveBlockTransactions(const Block& block);

        // it'll be ordered by descending fee rate for miner selection
        std::vector<Transaction> GetTransactionsSortedByFeeRate() const;

        std::map<std::string, Transaction> GetTransactions() const;
        std::vector<std::string> GetTransactionIDs() const;
        std::optional<Transaction> FindTransaction(const std::string& txid) const;
        bool Contains(const std::string& txid) const;
        size_t GetCount() const;
};

#endif