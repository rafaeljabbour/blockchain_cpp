#include "mempool.h"

#include <algorithm>
#include <iostream>

#include "serialization.h"

bool Mempool::EvictLowestFeeRate() {
    if (entries.empty()) return false;

    auto worst = entries.begin();
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        if (it->second.feeRate < worst->second.feeRate) {
            worst = it;
        }
    }

    totalBytes -= worst->second.txSize;
    std::cout << "[mempool] Evicted " << worst->first.substr(0, 16) << "..."
              << " feeRate=" << worst->second.feeRate << std::endl;
    entries.erase(worst);
    return true;
}

bool Mempool::AddTransaction(const Transaction& tx, double feeRate) {
    std::string txid = ByteArrayToHexString(tx.GetID());
    size_t txSize = tx.Serialize().size();

    std::lock_guard<std::mutex> lock(mtx);

    // already have it
    if (entries.count(txid)) return true;

    // evict until we're under both limits
    while ((totalBytes + txSize > Policy::MAX_MEMPOOL_SIZE ||
            entries.size() >= Policy::MAX_MEMPOOL_ENTRIES) &&
           !entries.empty()) {
        // don't evict if this tx has a lower fee rate than the worst entry
        auto worst = entries.begin();
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            if (it->second.feeRate < worst->second.feeRate) {
                worst = it;
            }
        }
        if (feeRate <= worst->second.feeRate) {
            std::cerr << "[mempool] Rejected " << txid.substr(0, 16) << "..."
                      << ": fee rate " << feeRate << " too low for full mempool" << std::endl;
            return false;
        }
        EvictLowestFeeRate();
    }

    entries[txid] = MempoolEntry{tx, feeRate, txSize};
    totalBytes += txSize;

    std::cout << "[mempool] Added " << txid.substr(0, 16) << "..."
              << " feeRate=" << feeRate << " raf/byte"
              << " (" << entries.size() << " txs, " << totalBytes / 1024 << " KB)" << std::endl;
    return true;
}

void Mempool::RemoveBlockTransactions(const Block& block) {
    std::lock_guard<std::mutex> lock(mtx);

    for (const auto& tx : block.GetTransactions()) {
        std::string txid = ByteArrayToHexString(tx.GetID());
        auto it = entries.find(txid);
        if (it != entries.end()) {
            totalBytes -= it->second.txSize;
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