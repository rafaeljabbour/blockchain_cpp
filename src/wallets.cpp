#include "wallets.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "config.h"
#include "serialization.h"

Wallets::Wallets(const std::string& passphrase) : passphrase(passphrase) {
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
    file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(fileSize));

    if (!file) {
        throw std::runtime_error("Failed to read wallet file");
    }

    // check if the file is encrypted: magic(4) + salt(16) + iv(16) + ciphertext
    bool encrypted = (fileSize >= 36 && std::memcmp(data.data(), WALLET_MAGIC, 4) == 0);

    if (encrypted) {
        if (passphrase.empty()) {
            throw std::runtime_error("Wallet file is encrypted. passphrase required");
        }

        // parse: magic(4) | salt(16) | iv(16) | ciphertext(rest)
        std::vector<uint8_t> salt(data.begin() + 4, data.begin() + 20);
        std::vector<uint8_t> iv(data.begin() + 20, data.begin() + 36);
        std::vector<uint8_t> ciphertext(data.begin() + 36, data.end());

        std::vector<uint8_t> plaintext = Decrypt(ciphertext, passphrase, salt, iv);
        Deserialize(plaintext);
    } else {
        Deserialize(data);
    }
}

// File format (encrypted):  magic(4) | salt(16) | iv(16) | ciphertext(...)
// File format (unencrypted): raw serialized wallet data
void Wallets::SaveToFile() const {
    std::vector<uint8_t> serialized = Serialize();

    std::vector<uint8_t> fileData;

    if (!passphrase.empty()) {
        // generate random salt
        std::vector<uint8_t> salt(16);
        if (RAND_bytes(salt.data(), 16) != 1) {
            throw std::runtime_error("Failed to generate random salt");
        }

        std::vector<uint8_t> iv;
        std::vector<uint8_t> ciphertext = Encrypt(serialized, passphrase, salt, iv);

        // assemble: magic | salt | iv | ciphertext
        fileData.insert(fileData.end(), WALLET_MAGIC, WALLET_MAGIC + 4);
        fileData.insert(fileData.end(), salt.begin(), salt.end());
        fileData.insert(fileData.end(), iv.begin(), iv.end());
        fileData.insert(fileData.end(), ciphertext.begin(), ciphertext.end());
    } else {
        fileData = std::move(serialized);
    }

    std::string walletPath = Config::GetWalletPath();
    std::filesystem::create_directories(std::filesystem::path(walletPath).parent_path());

    std::ofstream file(walletPath, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw std::runtime_error("Failed to open wallet file for writing");
    }

    file.write(reinterpret_cast<const char*>(fileData.data()),
               static_cast<std::streamsize>(fileData.size()));

    if (!file) {
        throw std::runtime_error("Failed to write wallet file");
    }
}

bool Wallets::WalletFileExists() { return std::filesystem::exists(Config::GetWalletPath()); }

bool Wallets::IsFileEncrypted() {
    std::string path = Config::GetWalletPath();
    if (!std::filesystem::exists(path)) return false;

    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    uint8_t magic[4]{};
    file.read(reinterpret_cast<char*>(magic), 4);
    if (!file || file.gcount() < 4) return false;

    return std::memcmp(magic, WALLET_MAGIC, 4) == 0;
}

// PBKDF2-HMAC-SHA256 key derivation to 32-byte AES-256 key
std::vector<uint8_t> Wallets::DeriveKey(const std::string& passphrase,
                                        const std::vector<uint8_t>& salt) {
    std::vector<uint8_t> key(32);

    if (PKCS5_PBKDF2_HMAC(passphrase.data(), static_cast<int>(passphrase.size()), salt.data(),
                          static_cast<int>(salt.size()), static_cast<int>(WALLET_KDF_ITERATIONS),
                          EVP_sha256(), 32, key.data()) != 1) {
        throw std::runtime_error("PBKDF2 key derivation failed");
    }

    return key;
}

// AES-256-CBC encrypt, writes the generated Iinitialization Vector into initVectorOut
std::vector<uint8_t> Wallets::Encrypt(const std::vector<uint8_t>& plaintext,
                                      const std::string& passphrase,
                                      const std::vector<uint8_t>& salt,
                                      std::vector<uint8_t>& initVectorOut) {
    std::vector<uint8_t> key = DeriveKey(passphrase, salt);

    initVectorOut.resize(16);
    if (RAND_bytes(initVectorOut.data(), 16) != 1) {
        throw std::runtime_error("Failed to generate random IV");
    }

    using EVP_CIPHER_CTX_ptr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;
    EVP_CIPHER_CTX_ptr ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!ctx) throw std::runtime_error("Failed to create cipher context");

    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_cbc(), nullptr, key.data(),
                           initVectorOut.data()) != 1) {
        throw std::runtime_error("EVP_EncryptInit_ex failed");
    }

    // output can be at most plaintext.size() + block_size
    std::vector<uint8_t> ciphertext(plaintext.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc()));
    int outLen = 0;

    if (EVP_EncryptUpdate(ctx.get(), ciphertext.data(), &outLen, plaintext.data(),
                          static_cast<int>(plaintext.size())) != 1) {
        throw std::runtime_error("EVP_EncryptUpdate failed");
    }

    int finalLen = 0;
    if (EVP_EncryptFinal_ex(ctx.get(), ciphertext.data() + outLen, &finalLen) != 1) {
        throw std::runtime_error("EVP_EncryptFinal_ex failed");
    }

    ciphertext.resize(static_cast<size_t>(outLen + finalLen));
    return ciphertext;
}

// AES-256-CBC decrypt
std::vector<uint8_t> Wallets::Decrypt(const std::vector<uint8_t>& ciphertext,
                                      const std::string& passphrase,
                                      const std::vector<uint8_t>& salt,
                                      const std::vector<uint8_t>& iv) {
    std::vector<uint8_t> key = DeriveKey(passphrase, salt);

    using EVP_CIPHER_CTX_ptr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;
    EVP_CIPHER_CTX_ptr ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!ctx) throw std::runtime_error("Failed to create cipher context");

    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_cbc(), nullptr, key.data(), iv.data()) != 1) {
        throw std::runtime_error("EVP_DecryptInit_ex failed");
    }

    std::vector<uint8_t> plaintext(ciphertext.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc()));
    int outLen = 0;

    if (EVP_DecryptUpdate(ctx.get(), plaintext.data(), &outLen, ciphertext.data(),
                          static_cast<int>(ciphertext.size())) != 1) {
        throw std::runtime_error("EVP_DecryptUpdate failed");
    }

    int finalLen = 0;
    if (EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + outLen, &finalLen) != 1) {
        throw std::runtime_error("Wallet decryption failed. wrong passphrase?");
    }

    plaintext.resize(static_cast<size_t>(outLen + finalLen));
    return plaintext;
}

void Wallets::EncryptAndSave(const std::string& newPassphrase) {
    if (newPassphrase.empty()) {
        throw std::runtime_error("Passphrase must not be empty");
    }

    passphrase = newPassphrase;
    SaveToFile();
}

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
        if (addressLen > serialized.size() - offset) {
            throw std::runtime_error("Wallet file corrupted: address data truncated");
        }
        std::string address(serialized.begin() + offset, serialized.begin() + offset + addressLen);
        offset += addressLen;

        // private key length (4 bytes)
        uint32_t privKeyLen = ReadUint32(serialized, offset);
        offset += 4;

        // private key bytes (variable bytes), the secp256k1 private key is 32 bytes
        if (privKeyLen > serialized.size() - offset) {
            throw std::runtime_error("Wallet file corrupted: private key data truncated");
        }
        if (privKeyLen != 32) {
            throw std::runtime_error("Wallet file corrupted: expected 32-byte private key, got " +
                                     std::to_string(privKeyLen));
        }
        std::vector<uint8_t> privKeyBytes(serialized.begin() + offset,
                                          serialized.begin() + offset + privKeyLen);
        offset += privKeyLen;

        // public key length (4 bytes)
        uint32_t pubKeyLen = ReadUint32(serialized, offset);
        offset += 4;

        // public key bytes (variable bytes), the secp256k1 public key is 33 (compressed) or 65
        // (uncompressed)
        if (pubKeyLen > serialized.size() - offset) {
            throw std::runtime_error("Wallet file corrupted: public key data truncated");
        }
        if (pubKeyLen != 33 && pubKeyLen != 65) {
            throw std::runtime_error(
                "Wallet file corrupted: expected 33 or 65-byte public key, got " +
                std::to_string(pubKeyLen));
        }
        std::vector<uint8_t> pubKeyBytes(serialized.begin() + offset,
                                         serialized.begin() + offset + pubKeyLen);
        offset += pubKeyLen;

        // Create wallet from bytes (Wallets is a friend)
        auto wallet = std::unique_ptr<Wallet>(new Wallet(privKeyBytes, pubKeyBytes));
        wallets[address] = std::move(wallet);
    }
}