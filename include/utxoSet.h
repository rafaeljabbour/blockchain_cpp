#ifndef UTXOSET_H
#define UTXOSET_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "transactionOutput.h"

class Blockchain;
class Block;
class Transaction;

// cache for quick blockchain transaction lookups (A set of unspent transaction outputs)
class UTXOSet {
        friend class Transaction;

    private:
        Blockchain* blockchain;

    public:
        explicit UTXOSet(Blockchain* bc);
        ~UTXOSet() = default;

        // prevent copying
        UTXOSet(const UTXOSet&) = delete;
        UTXOSet& operator=(const UTXOSet&) = delete;

        std::pair<int, std::map<std::string, std::vector<int>>> FindSpendableOutputs(
            const std::vector<uint8_t>& pubKeyHash, int amount) const;

        std::vector<TransactionOutput> FindUTXO(const std::vector<uint8_t>& pubKeyHash) const;

        int CountTransactions() const;

        void Reindex();

        void Update(const Block& block);
};

#endif