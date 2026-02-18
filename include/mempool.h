#ifndef MEMPOOL_H
#define MEMPOOL_H

#include <map>
#include <mutex>
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
        size_t GetCount() const;
};

#endif