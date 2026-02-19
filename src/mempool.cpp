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

// TODO: not sure yet how useful this is once I want to add more RPC apis
std::vector<std::string> Mempool::GetTransactionIDs() const {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<std::string> ids;
    ids.reserve(transactions.size());

    for (const auto& [txid, _] : transactions) {
        ids.push_back(txid);
    }

    return ids;
}

std::optional<Transaction> Mempool::FindTransaction(const std::string& txid) const {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = transactions.find(txid);
    if (it != transactions.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool Mempool::Contains(const std::string& txid) const {
    std::lock_guard<std::mutex> lock(mtx);
    return transactions.find(txid) != transactions.end();
}

size_t Mempool::GetCount() const {
    std::lock_guard<std::mutex> lock(mtx);
    return transactions.size();
}