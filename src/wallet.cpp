#include "wallet.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>

#include <cstring>
#include <iostream>

#include "base58.h"
#include "utils.h"

Wallet::Wallet() {
    auto [privateKey, publicKey] = NewKeyPair();
    this->privateKey = privateKey;
    this->publicKey = publicKey;
}

Wallet::~Wallet() {
    if (privateKey) {
        EVP_PKEY_free(privateKey);
    }
}

// generates a new ECDSA key pair using secp256k1 curve
std::pair<EVP_PKEY*, std::vector<uint8_t>> Wallet::NewKeyPair() {
    EVP_PKEY* pkey = nullptr;
    EVP_PKEY_CTX* ctx = nullptr;

    // create eleptical curve workspace
    ctx = EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr);
    if (!ctx) {
        std::cerr << "Error: Failed to create EVP_PKEY_CTX" << std::endl;
        exit(1);
    }

    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        std::cerr << "Error: Failed to initialize keygen" << std::endl;
        EVP_PKEY_CTX_free(ctx);
        exit(1);
    }

    OSSL_PARAM params[2];
    // set the curve to be a secp256k1 curve
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
                                                 const_cast<char*>("secp256k1"), 0);
    params[1] = OSSL_PARAM_construct_end();

    if (EVP_PKEY_CTX_set_params(ctx, params) <= 0) {
        std::cerr << "Error: Failed to set secp256k1 curve" << std::endl;
        EVP_PKEY_CTX_free(ctx);
        exit(1);
    }

    // generate the prive and public key container
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        std::cerr << "Error: Failed to generate key pair" << std::endl;
        EVP_PKEY_CTX_free(ctx);
        exit(1);
    }

    EVP_PKEY_CTX_free(ctx);

    // get the length of public key
    size_t pubKeyLen = 0;
    if (EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PUB_KEY, nullptr, 0, &pubKeyLen) <=
        0) {
        std::cerr << "Error: Failed to get public key length" << std::endl;
        EVP_PKEY_free(pkey);
        exit(1);
    }

    // extract public key from container
    std::vector<uint8_t> pubKey(pubKeyLen);
    if (EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PUB_KEY, pubKey.data(), pubKeyLen,
                                        nullptr) <= 0) {
        std::cerr << "Error: Failed to extract public key" << std::endl;
        EVP_PKEY_free(pkey);
        exit(1);
    }

    // return the container and public key
    return {pkey, pubKey};
}

std::vector<uint8_t> Wallet::GetAddress() const {
    std::vector<uint8_t> pubKeyHash = HashPubKey(publicKey);

    std::vector<uint8_t> address;
    address.push_back(VERSION);
    address.insert(address.end(), pubKeyHash.begin(), pubKeyHash.end());

    std::vector<uint8_t> checksum = Checksum(address);

    address.insert(address.end(), checksum.begin(), checksum.end());

    return Base58Encode(address);
}

std::vector<uint8_t> Wallet::HashPubKey(const std::vector<uint8_t>& pubKey) {
    return RIPEMD160Hash(SHA256Hash(pubKey));
}

// verify the creation of address using the checksum
bool Wallet::ValidateAddress(const std::string& address) {
    std::vector<uint8_t> decoded = Base58DecodeStr(address);

    // at least version (1 byte) + checksum (4 bytes)
    if (decoded.size() < ADDRESS_CHECKSUM_LEN + 1) {
        return false;
    }

    // using actual address
    std::vector<uint8_t> actualChecksum(decoded.end() - ADDRESS_CHECKSUM_LEN, decoded.end());
    uint8_t version = decoded[0];
    std::vector<uint8_t> pubKeyHash(decoded.begin() + 1, decoded.end() - ADDRESS_CHECKSUM_LEN);

    // rebuilding address to checksum
    std::vector<uint8_t> versionedPayload;
    versionedPayload.push_back(version);
    versionedPayload.insert(versionedPayload.end(), pubKeyHash.begin(), pubKeyHash.end());

    std::vector<uint8_t> targetChecksum = Checksum(versionedPayload);

    return actualChecksum == targetChecksum;
}

// Calculates the checksum of an address
std::vector<uint8_t> Wallet::Checksum(const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> firstHash = SHA256Hash(payload);
    std::vector<uint8_t> secondHash = SHA256Hash(firstHash);
    return std::vector<uint8_t>(secondHash.begin(), secondHash.begin() + ADDRESS_CHECKSUM_LEN);
}

Wallet::Wallet(const std::vector<uint8_t>& privKeyBytes, const std::vector<uint8_t>& pubKeyBytes)
    : privateKey(nullptr), publicKey(pubKeyBytes) {
    // we reconstruct private key from bytes using OpenSSL
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr);
    if (!ctx) {
        std::cerr << "Error: Failed to create EVP_PKEY_CTX for deserialization" << std::endl;
        exit(1);
    }

    if (EVP_PKEY_fromdata_init(ctx) <= 0) {
        std::cerr << "Error: Failed to initialize fromdata" << std::endl;
        EVP_PKEY_CTX_free(ctx);
        exit(1);
    }

    OSSL_PARAM params[3];
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
                                                 const_cast<char*>("secp256k1"), 0);
    params[1] = OSSL_PARAM_construct_octet_string(
        OSSL_PKEY_PARAM_PRIV_KEY, const_cast<uint8_t*>(privKeyBytes.data()), privKeyBytes.size());
    params[2] = OSSL_PARAM_construct_end();

    if (EVP_PKEY_fromdata(ctx, &privateKey, EVP_PKEY_KEYPAIR, params) <= 0) {
        std::cerr << "Error: Failed to reconstruct private key from bytes" << std::endl;
        EVP_PKEY_CTX_free(ctx);
        exit(1);
    }

    EVP_PKEY_CTX_free(ctx);
}

std::vector<uint8_t> Wallet::GetPrivateKeyBytes() const {
    size_t privKeyLen = 0;

    // get the length of the private key
    if (EVP_PKEY_get_octet_string_param(privateKey, OSSL_PKEY_PARAM_PRIV_KEY, nullptr, 0,
                                        &privKeyLen) <= 0) {
        std::cerr << "Error: Failed to get private key length" << std::endl;
        exit(1);
    }

    // extract the private key from container
    std::vector<uint8_t> privKey(privKeyLen);
    if (EVP_PKEY_get_octet_string_param(privateKey, OSSL_PKEY_PARAM_PRIV_KEY, privKey.data(),
                                        privKeyLen, nullptr) <= 0) {
        std::cerr << "Error: Failed to extract private key bytes" << std::endl;
        exit(1);
    }

    return privKey;
}