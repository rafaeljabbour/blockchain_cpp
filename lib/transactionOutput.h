#ifndef TRANSACTIONOUTPUT_H
#define TRANSACTIONOUTPUT_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class TransactionOutput {
    private:
        int value;
        std::string scriptPubKey;

    public:
        TransactionOutput() = default;
        TransactionOutput(int value, const std::string& scriptPubKey);

        int GetValue() const { return value; }
        const std::string& GetScriptPubKey() const { return scriptPubKey; }

        bool CanBeUnlockedWith(const std::string& unlockingData) const;

        std::vector<uint8_t> Serialize() const;
        static std::pair<TransactionOutput, size_t> Deserialize(const std::vector<uint8_t>& data,
                                                                size_t offset);
};

#endif