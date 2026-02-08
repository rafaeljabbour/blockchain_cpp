#ifndef WALLETS_H
#define WALLETS_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "wallet.h"

class Wallets {
    private:
        std::map<std::string, std::unique_ptr<Wallet>> wallets;

        std::vector<uint8_t> Serialize() const;
        void Deserialize(const std::vector<uint8_t>& serialized);

    public:
        Wallets();
        ~Wallets() = default;

        // Removing default copy operators
        Wallets(const Wallets&) = delete;
        Wallets& operator=(const Wallets&) = delete;

        std::string CreateWallet();

        std::vector<std::string> GetAddresses() const;

        Wallet* GetWallet(const std::string& address);

        void LoadFromFile();
        void SaveToFile() const;
        static bool WalletFileExists();
};

#endif