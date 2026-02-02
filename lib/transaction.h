#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <string>
#include <vector>

#include "transactionInput.h"
#include "transactionOutput.h"

const int SUBSIDY = 10;

class Blockchain;

class Transaction {
    private:
        std::vector<uint8_t> id;
        std::vector<TransactionInput> vin;
        std::vector<TransactionOutput> vout;

        Transaction() = default;
        Transaction(const std::vector<uint8_t>& id, const std::vector<TransactionInput>& vin,
                    const std::vector<TransactionOutput>& vout);

        void SetID();

    public:
        const std::vector<uint8_t>& GetID() const { return id; }
        const std::vector<TransactionInput>& GetVin() const { return vin; }
        const std::vector<TransactionOutput>& GetVout() const { return vout; }

        bool IsCoinbase() const;

        static Transaction NewCoinbaseTX(const std::string& to, const std::string& data = "");
        static Transaction NewUTXOTransaction(const std::string& from, const std::string& to,
                                              int amount, Blockchain* bc);

        std::vector<uint8_t> Serialize() const;
        static Transaction Deserialize(const std::vector<uint8_t>& data);
};

#endif