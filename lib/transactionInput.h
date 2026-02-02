#ifndef TRANSACTIONINPUT_H
#define TRANSACTIONINPUT_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class TransactionInput {
    private:
        std::vector<uint8_t> txid;
        int vout;
        std::string scriptSig;

    public:
        TransactionInput() = default;
        TransactionInput(const std::vector<uint8_t>& txid, int vout, const std::string& scriptSig);

        const std::vector<uint8_t>& GetTxid() const { return txid; }
        int GetVout() const { return vout; }
        const std::string& GetScriptSig() const { return scriptSig; }

        bool CanUnlockOutputWith(const std::string& unlockingData) const;

        std::vector<uint8_t> Serialize() const;
        static std::pair<TransactionInput, size_t> Deserialize(const std::vector<uint8_t>& data,
                                                               size_t offset);
};

#endif