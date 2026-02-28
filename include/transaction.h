#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <openssl/evp.h>

#include <map>
#include <string>
#include <vector>

#include "transactionInput.h"
#include "transactionOutput.h"

const int SUBSIDY = 10;

class Blockchain;
class UTXOSet;

class Transaction {
    private:
        std::vector<uint8_t> id;
        std::vector<TransactionInput> vin;
        std::vector<TransactionOutput> vout;

    public:
        Transaction() = default;
        Transaction(const std::vector<uint8_t>& id, const std::vector<TransactionInput>& vin,
                    const std::vector<TransactionOutput>& vout);
        const std::vector<uint8_t>& GetID() const { return id; }
        const std::vector<TransactionInput>& GetVin() const { return vin; }
        const std::vector<TransactionOutput>& GetVout() const { return vout; }

        bool IsCoinbase() const;

        std::vector<uint8_t> Hash() const;
        void Sign(EVP_PKEY* privKey, const std::map<std::string, Transaction>& prevTXs);
        bool Verify(const std::map<std::string, Transaction>& prevTXs) const;

        // fee = sum(input values) - sum(output values)
        int64_t CalculateFee(const std::map<std::string, Transaction>& prevTXs) const;

        Transaction TrimmedCopy() const;

        static Transaction NewCoinbaseTX(const std::string& to, int64_t fees = 0,
                                         const std::string& data = "");
        static Transaction NewUTXOTransaction(const std::string& from, const std::string& to,
                                              int amount, UTXOSet* utxoSet);

        std::vector<uint8_t> Serialize() const;
        static Transaction Deserialize(const std::vector<uint8_t>& data);
};

#endif