#include "mempool.h"

#include <algorithm>
#include <iostream>

#include "serialization.h"

void Mempool::AddTransaction(const Transaction& tx, double feeRate) {
    std::string txid = ByteArrayToHexString(tx.GetID());

    std::lock_guard<std::mutex> lock(mtx);
    entries[txid] = MempoolEntry{tx, feeRate};

    std::cout << "[mempool] Added transaction " << txid.substr(0, 16) << "..."
              << " feeRate=" << feeRate << " raf/byte"
              << " (" << entries.size() << " total)" << std::endl;
}

void Mempool::RemoveBlockTransactions(const Block& block) {
    std::lock_guard<std::mutex> lock(mtx);

    for (const auto& tx : block.GetTransactions()) {
        std::string txid = ByteArrayToHexString(tx.GetID());
        auto it = entries.find(txid);
        if (it != entries.end()) {
            entries.erase(it);
            std::cout << "[mempool] Removed mined transaction " << txid << std::endl;
        }
    }
}

std::vector<Transaction> Mempool::GetTransactionsSortedByFeeRate() const {
    std::lock_guard<std::mutex> lock(mtx);

    std::vector<const MempoolEntry*> ptrs;
    ptrs.reserve(entries.size());
    for (const auto& [_, entry] : entries) {
        ptrs.push_back(&entry);
    }

    // we sort the transactions by descending fee rate
    std::sort(ptrs.begin(), ptrs.end(),
              [](const MempoolEntry* a, const MempoolEntry* b) { return a->feeRate > b->feeRate; });

    std::vector<Transaction> result;
    result.reserve(ptrs.size());
    for (const auto* entry : ptrs) {
        result.push_back(entry->tx);
    }
    return result;
}

std::map<std::string, Transaction> Mempool::GetTransactions() const {
    std::lock_guard<std::mutex> lock(mtx);
    std::map<std::string, Transaction> txs;
    for (const auto& [txid, entry] : entries) {
        txs[txid] = entry.tx;
    }
    return txs;
}

// TODO: not sure yet how useful this is once I want to add more RPC apis
std::vector<std::string> Mempool::GetTransactionIDs() const {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<std::string> ids;
    ids.reserve(entries.size());

    for (const auto& [txid, _] : entries) {
        ids.push_back(txid);
    }

    return ids;
}

std::optional<Transaction> Mempool::FindTransaction(const std::string& txid) const {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = entries.find(txid);
    if (it != entries.end()) {
        return it->second.tx;
    }
    return std::nullopt;
}

bool Mempool::Contains(const std::string& txid) const {
    std::lock_guard<std::mutex> lock(mtx);
    return entries.find(txid) != entries.end();
}

size_t Mempool::GetCount() const {
    std::lock_guard<std::mutex> lock(mtx);
    return entries.size();
}