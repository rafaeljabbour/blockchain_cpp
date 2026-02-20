#ifndef MEMPOOL_H
#define MEMPOOL_H

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "block.h"
#include "transaction.h"

// stores unconfirmed transactions.
class Mempool {
    private:
        // the txid in hex gives us the transaction
        std::map<std::string, Transaction> transactions;
        mutable std::mutex mtx;

    public:
        Mempool() = default;

        void AddTransaction(const Transaction& tx);
        void RemoveBlockTransactions(const Block& block);
        std::map<std::string, Transaction> GetTransactions() const;
        std::vector<std::string> GetTransactionIDs() const;
        std::optional<Transaction> FindTransaction(const std::string& txid) const;
        bool Contains(const std::string& txid) const;
        size_t GetCount() const;
};

#endif