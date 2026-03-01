#ifndef TRANSACTIONOUTPUT_H
#define TRANSACTIONOUTPUT_H

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

class TransactionOutput {
    private:
        int64_t value;
        std::vector<uint8_t> pubKeyHash;

    public:
        TransactionOutput() = default;
        TransactionOutput(int64_t value, const std::vector<uint8_t>& pubKeyHash);

        int64_t GetValue() const { return value; }
        const std::vector<uint8_t>& GetPubKeyHash() const { return pubKeyHash; }

        void Lock(const std::vector<uint8_t>& address);

        bool IsLockedWithKey(const std::vector<uint8_t>& pubKeyHash) const;

        std::vector<uint8_t> Serialize() const;
        static std::pair<TransactionOutput, size_t> Deserialize(const std::vector<uint8_t>& data,
                                                                size_t offset);
};

// struct to store multiple outputs keyed by their original transaction output index
struct TXOutputs {
        std::map<int, TransactionOutput> outputs;

        std::vector<uint8_t> Serialize() const;
        static TXOutputs Deserialize(const std::vector<uint8_t>& data);
};

// factory function to make object creation easier
TransactionOutput NewTXOutput(int64_t value, const std::string& address);

#endif