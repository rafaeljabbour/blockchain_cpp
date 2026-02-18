#include "mempool.h"

#include <iostream>

#include "utils.h"

void Mempool::AddTransaction(const Transaction& tx) {
    std::string txid = ByteArrayToHexString(tx.GetID());

    std::lock_guard<std::mutex> lock(mtx);
    transactions[txid] = tx;

    std::cout << "[mempool] Added transaction " << txid << " (" << transactions.size() << " total)"
              << std::endl;
}

void Mempool::RemoveBlockTransactions(const Block& block) {
    std::lock_guard<std::mutex> lock(mtx);

    for (const auto& tx : block.GetTransactions()) {
        std::string txid = ByteArrayToHexString(tx.GetID());
        auto it = transactions.find(txid);
        if (it != transactions.end()) {
            transactions.erase(it);
            std::cout << "[mempool] Removed mined transaction " << txid << std::endl;
        }
    }
}

std::map<std::string, Transaction> Mempool::GetTransactions() const {
    std::lock_guard<std::mutex> lock(mtx);
    return transactions;
}

size_t Mempool::GetCount() const {
    std::lock_guard<std::mutex> lock(mtx);
    return transactions.size();
}