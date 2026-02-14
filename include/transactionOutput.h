#ifndef TRANSACTIONOUTPUT_H
#define TRANSACTIONOUTPUT_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class TransactionOutput {
    private:
        int value;
        std::vector<uint8_t> pubKeyHash;

    public:
        TransactionOutput() = default;
        TransactionOutput(int value, const std::vector<uint8_t>& pubKeyHash);

        int GetValue() const { return value; }
        const std::vector<uint8_t>& GetPubKeyHash() const { return pubKeyHash; }

        void Lock(const std::vector<uint8_t>& address);

        bool IsLockedWithKey(const std::vector<uint8_t>& pubKeyHash) const;

        std::vector<uint8_t> Serialize() const;
        static std::pair<TransactionOutput, size_t> Deserialize(const std::vector<uint8_t>& data,
                                                                size_t offset);
};

// struct to store multiple outputs, under a single transaction ID in the db
struct TXOutputs {
        std::vector<TransactionOutput> outputs;

        std::vector<uint8_t> Serialize() const;
        static TXOutputs Deserialize(const std::vector<uint8_t>& data);
};

// factory function to make object creation easier
TransactionOutput NewTXOutput(int value, const std::string& address);

#endif