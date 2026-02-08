#ifndef TRANSACTIONINPUT_H
#define TRANSACTIONINPUT_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class TransactionInput {
        friend class Transaction;

    private:
        std::vector<uint8_t> txid;
        int vout;
        std::vector<uint8_t> signature;
        std::vector<uint8_t> pubKey;

    public:
        TransactionInput() = default;
        TransactionInput(const std::vector<uint8_t>& txid, int vout,
                         const std::vector<uint8_t>& signature, const std::vector<uint8_t>& pubKey);

        const std::vector<uint8_t>& GetTxid() const { return txid; }
        int GetVout() const { return vout; }

        const std::vector<uint8_t>& GetSignature() const { return signature; }
        const std::vector<uint8_t>& GetPubKey() const { return pubKey; }

        bool UsesKey(const std::vector<uint8_t>& pubKeyHash) const;

        std::vector<uint8_t> Serialize() const;
        static std::pair<TransactionInput, size_t> Deserialize(const std::vector<uint8_t>& data,
                                                               size_t offset);
};

#endif