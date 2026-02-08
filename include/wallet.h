#ifndef WALLET_H
#define WALLET_H

#include <openssl/evp.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

const uint8_t VERSION = 0x00;
const std::string WALLET_FILE = "./data/wallet.dat";
const int ADDRESS_CHECKSUM_LEN = 4;

// RAII type alias for ownership
using EVP_PKEY_owned = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;

class Wallet {
        friend class Blockchain;
        friend class Wallets;

    private:
        EVP_PKEY_owned privateKey;
        std::vector<uint8_t> publicKey;

        static std::pair<EVP_PKEY_owned, std::vector<uint8_t>> NewKeyPair();
        static std::vector<uint8_t> Checksum(const std::vector<uint8_t>& payload);
        std::vector<uint8_t> GetPrivateKeyBytes() const;
        Wallet(const std::vector<uint8_t>& privKeyBytes, const std::vector<uint8_t>& pubKeyBytes);

    public:
        Wallet();
        ~Wallet() = default;

        // prevent copying
        Wallet(const Wallet&) = delete;
        Wallet& operator=(const Wallet&) = delete;

        std::vector<uint8_t> GetAddress() const;
        const std::vector<uint8_t>& GetPublicKey() const { return publicKey; }

        static std::vector<uint8_t> HashPubKey(const std::vector<uint8_t>& pubKey);
        static bool ValidateAddress(const std::string& address);
};

#endif