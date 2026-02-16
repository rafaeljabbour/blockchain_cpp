#include "wallets.h"

#include <filesystem>
#include <fstream>

#include "config.h"
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
    std::string walletPath = Config::GetWalletPath();

    if (!std::filesystem::exists(walletPath)) {
        // File doesn't exist, which is fine for first run
        return;
    }

    size_t fileSize = std::filesystem::file_size(walletPath);

    if (fileSize == 0) {
        return;
    }

    std::ifstream file(walletPath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open wallet file for reading");
    }

    std::vector<uint8_t> data(fileSize);
    file.read(reinterpret_cast<char*>(data.data()), fileSize);

    if (!file) {
        throw std::runtime_error("Failed to read wallet file");
    }

    Deserialize(data);
}

void Wallets::SaveToFile() const {
    std::vector<uint8_t> data = Serialize();

    std::string walletPath = Config::GetWalletPath();
    std::filesystem::create_directories(std::filesystem::path(walletPath).parent_path());

    std::ofstream file(walletPath, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw std::runtime_error("Failed to open wallet file for writing");
    }

    file.write(reinterpret_cast<const char*>(data.data()), data.size());

    if (!file) {
        throw std::runtime_error("Failed to write wallet file");
    }
}

bool Wallets::WalletFileExists() { return std::filesystem::exists(Config::GetWalletPath()); }

std::vector<uint8_t> Wallets::Serialize() const {
    std::vector<uint8_t> serialized;

    // number of wallets (4 bytes)
    WriteUint32(serialized, static_cast<uint32_t>(wallets.size()));

    // each wallet
    for (const auto& [address, wallet] : wallets) {
        // Get private and public key bytes
        std::vector<uint8_t> privKeyBytes = wallet->GetPrivateKeyBytes();
        const std::vector<uint8_t>& pubKeyBytes = wallet->GetPublicKey();

        // address length (4 bytes)
        WriteUint32(serialized, static_cast<uint32_t>(address.length()));
        // address (variable bytes)
        serialized.insert(serialized.end(), address.begin(), address.end());

        // private key length (4 bytes)
        WriteUint32(serialized, static_cast<uint32_t>(privKeyBytes.size()));
        // private key bytes (variable bytes)
        serialized.insert(serialized.end(), privKeyBytes.begin(), privKeyBytes.end());

        // public key length (4 bytes)
        WriteUint32(serialized, static_cast<uint32_t>(pubKeyBytes.size()));
        // public key bytes (variable bytes)
        serialized.insert(serialized.end(), pubKeyBytes.begin(), pubKeyBytes.end());
    }

    return serialized;
}

void Wallets::Deserialize(const std::vector<uint8_t>& serialized) {
    size_t offset = 0;

    // number of wallets (4 bytes)
    uint32_t walletCount = ReadUint32(serialized, offset);
    offset += 4;

    // Deserialize each wallet
    for (uint32_t i = 0; i < walletCount; i++) {
        // address length (4 bytes)
        uint32_t addressLen = ReadUint32(serialized, offset);
        offset += 4;

        // address (variable bytes)
        if (offset + addressLen > serialized.size()) {
            throw std::runtime_error("Wallet file corrupted: address data truncated");
        }
        std::string address(serialized.begin() + offset, serialized.begin() + offset + addressLen);
        offset += addressLen;

        // private key length (4 bytes)
        uint32_t privKeyLen = ReadUint32(serialized, offset);
        offset += 4;

        // private key bytes (variable bytes)
        if (offset + privKeyLen > serialized.size()) {
            throw std::runtime_error("Wallet file corrupted: private key data truncated");
        }
        std::vector<uint8_t> privKeyBytes(serialized.begin() + offset,
                                          serialized.begin() + offset + privKeyLen);
        offset += privKeyLen;

        // public key length (4 bytes)
        uint32_t pubKeyLen = ReadUint32(serialized, offset);
        offset += 4;

        // public key bytes (variable bytes)
        if (offset + pubKeyLen > serialized.size()) {
            throw std::runtime_error("Wallet file corrupted: public key data truncated");
        }
        std::vector<uint8_t> pubKeyBytes(serialized.begin() + offset,
                                         serialized.begin() + offset + pubKeyLen);
        offset += pubKeyLen;

        // Create wallet from bytes (Wallets is a friend)
        auto wallet = std::unique_ptr<Wallet>(new Wallet(privKeyBytes, pubKeyBytes));
        wallets[address] = std::move(wallet);
    }
}