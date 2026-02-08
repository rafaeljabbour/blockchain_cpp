#ifndef WALLET_H
#define WALLET_H

#include <openssl/evp.h>

#include <cstdint>
#include <string>
#include <vector>

const uint8_t VERSION = 0x00;
const std::string WALLET_FILE = "wallet.dat";
const int ADDRESS_CHECKSUM_LEN = 4;

class Wallet {
        friend class Wallets;

    private:
        EVP_PKEY* privateKey;  // Note: EC_KEY is deprecated
        std::vector<uint8_t> publicKey;

        static std::pair<EVP_PKEY*, std::vector<uint8_t>> NewKeyPair();
        static std::vector<uint8_t> Checksum(const std::vector<uint8_t>& payload);
        std::vector<uint8_t> GetPrivateKeyBytes() const;
        Wallet(const std::vector<uint8_t>& privKeyBytes, const std::vector<uint8_t>& pubKeyBytes);

    public:
        Wallet();
        ~Wallet();

        // Removing default copy operators
        Wallet(const Wallet&) = delete;
        Wallet& operator=(const Wallet&) = delete;

        std::vector<uint8_t> GetAddress() const;
        const std::vector<uint8_t>& GetPublicKey() const { return publicKey; }

        static std::vector<uint8_t> HashPubKey(const std::vector<uint8_t>& pubKey);
        static bool ValidateAddress(const std::string& address);
};

#endif