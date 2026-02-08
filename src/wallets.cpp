#include "wallets.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#include "utils.h"

Wallets::Wallets() {
    if (WalletFileExists()) {
        LoadFromFile();
    }
}

std::string Wallets::CreateWallet() {
    auto wallet = std::make_unique<Wallet>();
    std::string address = BytesToString(wallet->GetAddress());

    // store wallet
    wallets[address] = std::move(wallet);

    return address;
}

std::vector<std::string> Wallets::GetAddresses() const {
    std::vector<std::string> addresses;
    addresses.reserve(wallets.size());

    for (const auto& [address, wallet] : wallets) {
        addresses.push_back(address);
    }

    return addresses;
}

Wallet* Wallets::GetWallet(const std::string& address) {
    auto it = wallets.find(address);
    if (it != wallets.end()) {
        return it->second.get();
    }
    return nullptr;
}

void Wallets::LoadFromFile() {
    if (!std::filesystem::exists(WALLET_FILE)) {
        // File doesn't exist, which is fine for first run
        return;
    }

    size_t fileSize = std::filesystem::file_size(WALLET_FILE);

    if (fileSize == 0) {
        return;
    }

    std::ifstream file(WALLET_FILE, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Failed to open wallet file for reading" << std::endl;
        exit(1);
    }

    std::vector<uint8_t> data(fileSize);
    file.read(reinterpret_cast<char*>(data.data()), fileSize);
    file.close();

    if (!file) {
        std::cerr << "Error: Failed to read wallet file" << std::endl;
        exit(1);
    }

    Deserialize(data);
}

void Wallets::SaveToFile() const {
    std::vector<uint8_t> data = Serialize();

    std::ofstream file(WALLET_FILE, std::ios::binary | std::ios::trunc);
    if (!file) {
        std::cerr << "Error: Failed to open wallet file for writing" << std::endl;
        exit(1);
    }

    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    file.close();

    if (!file) {
        std::cerr << "Error: Failed to write wallet file" << std::endl;
        exit(1);
    }
}

bool Wallets::WalletFileExists() { return std::filesystem::exists(WALLET_FILE); }

std::vector<uint8_t> Wallets::Serialize() const {
    std::vector<uint8_t> serialized;

    // number of wallets (4 bytes)
    uint32_t walletCount = wallets.size();
    for (int i = 0; i < 4; i++) {
        serialized.push_back((walletCount >> (8 * i)) & 0xFF);
    }

    // each wallet
    for (const auto& [address, wallet] : wallets) {
        // Get private and public key bytes
        std::vector<uint8_t> privKeyBytes = wallet->GetPrivateKeyBytes();
        const std::vector<uint8_t>& pubKeyBytes = wallet->GetPublicKey();

        // address length (4 bytes)
        uint32_t addressLen = address.length();
        for (int i = 0; i < 4; i++) {
            serialized.push_back((addressLen >> (8 * i)) & 0xFF);
        }

        // address (variable bytes)
        serialized.insert(serialized.end(), address.begin(), address.end());

        // private key length (4 bytes)
        uint32_t privKeyLen = privKeyBytes.size();
        for (int i = 0; i < 4; i++) {
            serialized.push_back((privKeyLen >> (8 * i)) & 0xFF);
        }

        // private key bytes (variable bytes)
        serialized.insert(serialized.end(), privKeyBytes.begin(), privKeyBytes.end());

        // public key length (4 bytes)
        uint32_t pubKeyLen = pubKeyBytes.size();
        for (int i = 0; i < 4; i++) {
            serialized.push_back((pubKeyLen >> (8 * i)) & 0xFF);
        }

        // public key bytes (variable bytes)
        serialized.insert(serialized.end(), pubKeyBytes.begin(), pubKeyBytes.end());
    }

    return serialized;
}

void Wallets::Deserialize(const std::vector<uint8_t>& serialized) {
    size_t offset = 0;

    if (serialized.size() < 4) {
        std::cerr << "Error: Invalid wallet file format" << std::endl;
        exit(1);
    }

    // number of wallets (4 bytes)
    uint32_t walletCount = 0;
    for (int i = 0; i < 4; i++) {
        walletCount |= (serialized[offset + i] << (8 * i));
    }
    offset += 4;

    // Deserialize each wallet
    for (uint32_t i = 0; i < walletCount; i++) {
        // address length (4 bytes)
        uint32_t addressLen = 0;
        for (int j = 0; j < 4; j++) {
            addressLen |= (serialized[offset + j] << (8 * j));
        }
        offset += 4;

        // address (variable bytes)
        std::string address(serialized.begin() + offset, serialized.begin() + offset + addressLen);
        offset += addressLen;

        // private key length (4 bytes)
        uint32_t privKeyLen = 0;
        for (int j = 0; j < 4; j++) {
            privKeyLen |= (serialized[offset + j] << (8 * j));
        }
        offset += 4;

        // private key bytes (variable bytes)
        std::vector<uint8_t> privKeyBytes(serialized.begin() + offset,
                                          serialized.begin() + offset + privKeyLen);
        offset += privKeyLen;

        // Read public key length (4 bytes)
        uint32_t pubKeyLen = 0;
        for (int j = 0; j < 4; j++) {
            pubKeyLen |= (serialized[offset + j] << (8 * j));
        }
        offset += 4;

        // public key bytes (variable bytes)
        std::vector<uint8_t> pubKeyBytes(serialized.begin() + offset,
                                         serialized.begin() + offset + pubKeyLen);
        offset += pubKeyLen;

        // Create wallet from bytes (Wallets is a friend)
        auto wallet = std::unique_ptr<Wallet>(new Wallet(privKeyBytes, pubKeyBytes));
        wallets[address] = std::move(wallet);
    }
}