#ifndef WALLETS_H
#define WALLETS_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "wallet.h"

// encrypted wallet file magic bytes
inline constexpr uint8_t WALLET_MAGIC[4] = {'E', 'W', 'L', 'T'};

// PBKDF2 iterations (OWASP recommended minimum for HMAC-SHA256)
inline constexpr uint32_t WALLET_KDF_ITERATIONS = 600'000;

class Wallets {
    private:
        std::map<std::string, std::unique_ptr<Wallet>> wallets;
        std::string passphrase;  // held in memory while wallet is unlocked

        std::vector<uint8_t> Serialize() const;
        void Deserialize(const std::vector<uint8_t>& serialized);

        // AES-256-CBC encrypt/decrypt using a PBKDF2-derived key
        static std::vector<uint8_t> Encrypt(const std::vector<uint8_t>& plaintext,
                                            const std::string& passphrase,
                                            const std::vector<uint8_t>& salt,
                                            std::vector<uint8_t>& initVectorOut);
        static std::vector<uint8_t> Decrypt(const std::vector<uint8_t>& ciphertext,
                                            const std::string& passphrase,
                                            const std::vector<uint8_t>& salt,
                                            const std::vector<uint8_t>& iv);
        static std::vector<uint8_t> DeriveKey(const std::string& passphrase,
                                              const std::vector<uint8_t>& salt);

    public:
        // loads wallet file and requires passphrase if the file is encrypted
        explicit Wallets(const std::string& passphrase = "");
        ~Wallets() = default;

        Wallets(const Wallets&) = delete;
        Wallets& operator=(const Wallets&) = delete;

        std::string CreateWallet();

        std::vector<std::string> GetAddresses() const;

        Wallet* GetWallet(const std::string& address);

        void LoadFromFile();
        void SaveToFile() const;

        // encrypt an existing unencrypted wallet with a passphrase
        void EncryptAndSave(const std::string& newPassphrase);

        static bool WalletFileExists();
        static bool IsFileEncrypted();
};

#endif