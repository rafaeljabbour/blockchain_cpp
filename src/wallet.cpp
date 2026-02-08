#include "wallet.h"

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <openssl/params.h>

#include <memory>

#include "base58.h"
#include "utils.h"

// RAII type aliases resources
using EVP_PKEY_CTX_ptr = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using BN_ptr = std::unique_ptr<BIGNUM, decltype(&BN_free)>;
using OSSL_PARAM_BLD_ptr = std::unique_ptr<OSSL_PARAM_BLD, decltype(&OSSL_PARAM_BLD_free)>;
using OSSL_PARAM_ptr = std::unique_ptr<OSSL_PARAM, decltype(&OSSL_PARAM_free)>;

Wallet::Wallet() : privateKey(nullptr, EVP_PKEY_free) {
    auto [privateKey, publicKey] = NewKeyPair();
    this->privateKey = std::move(privateKey);
    this->publicKey = std::move(publicKey);
}

// generates a new ECDSA key pair using secp256k1 curve
std::pair<EVP_PKEY_owned, std::vector<uint8_t>> Wallet::NewKeyPair() {
    // create elliptical curve workspace
    EVP_PKEY_CTX_ptr ctx(EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr), EVP_PKEY_CTX_free);
    if (!ctx) {
        throw std::runtime_error("Failed to create EVP_PKEY_CTX");
    }

    if (EVP_PKEY_keygen_init(ctx.get()) <= 0) {
        throw std::runtime_error("Failed to initialize keygen");
    }

    OSSL_PARAM params[2];
    // set the curve to be a secp256k1 curve
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
                                                 const_cast<char*>("secp256k1"), 0);
    params[1] = OSSL_PARAM_construct_end();

    if (EVP_PKEY_CTX_set_params(ctx.get(), params) <= 0) {
        throw std::runtime_error("Failed to set secp256k1 curve");
    }

    // generate the private and public key container
    EVP_PKEY* rawKey = nullptr;
    if (EVP_PKEY_keygen(ctx.get(), &rawKey) <= 0) {
        throw std::runtime_error("Failed to generate key pair");
    }

    // wrap in RAII
    EVP_PKEY_owned pkey(rawKey, EVP_PKEY_free);

    // get the length of public key
    size_t pubKeyLen = 0;
    if (EVP_PKEY_get_octet_string_param(pkey.get(), OSSL_PKEY_PARAM_PUB_KEY, nullptr, 0,
                                        &pubKeyLen) <= 0) {
        throw std::runtime_error("Failed to get public key length");
    }

    // extract public key from container
    std::vector<uint8_t> pubKey(pubKeyLen);
    if (EVP_PKEY_get_octet_string_param(pkey.get(), OSSL_PKEY_PARAM_PUB_KEY, pubKey.data(),
                                        pubKeyLen, nullptr) <= 0) {
        throw std::runtime_error("Failed to extract public key");
    }

    return {std::move(pkey), pubKey};
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
    std::vector<uint8_t> decoded;
    try {
        decoded = Base58DecodeStr(address);
    } catch (const std::runtime_error&) {
        // invalid Base58 characters means invalid address
        return false;
    }

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
    : privateKey(nullptr, EVP_PKEY_free), publicKey(pubKeyBytes) {
    // convert private key bytes into a BIGNUM
    BN_ptr privBN(BN_bin2bn(privKeyBytes.data(), privKeyBytes.size(), nullptr), BN_free);
    if (!privBN) {
        throw std::runtime_error("Failed to convert private key bytes to BIGNUM");
    }

    // use the parameter builder API to handles BIGNUM encoding
    OSSL_PARAM_BLD_ptr bld(OSSL_PARAM_BLD_new(), OSSL_PARAM_BLD_free);
    if (!bld) {
        throw std::runtime_error("Failed to create OSSL_PARAM_BLD");
    }

    // add the parameters
    if (OSSL_PARAM_BLD_push_utf8_string(bld.get(), OSSL_PKEY_PARAM_GROUP_NAME, "secp256k1", 0) <=
            0 ||
        OSSL_PARAM_BLD_push_BN(bld.get(), OSSL_PKEY_PARAM_PRIV_KEY, privBN.get()) <= 0 ||
        OSSL_PARAM_BLD_push_octet_string(bld.get(), OSSL_PKEY_PARAM_PUB_KEY, pubKeyBytes.data(),
                                         pubKeyBytes.size()) <= 0) {
        throw std::runtime_error("Failed to set key parameters");
    }

    // create the parameter array
    OSSL_PARAM_ptr params(OSSL_PARAM_BLD_to_param(bld.get()), OSSL_PARAM_free);
    if (!params) {
        throw std::runtime_error("Failed to build parameters");
    }

    EVP_PKEY_CTX_ptr ctx(EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr), EVP_PKEY_CTX_free);
    if (!ctx) {
        throw std::runtime_error("Failed to create EVP_PKEY_CTX for deserialization");
    }

    if (EVP_PKEY_fromdata_init(ctx.get()) <= 0) {
        throw std::runtime_error("Failed to initialize fromdata");
    }

    EVP_PKEY* rawKey = nullptr;
    if (EVP_PKEY_fromdata(ctx.get(), &rawKey, EVP_PKEY_KEYPAIR, params.get()) <= 0) {
        throw std::runtime_error("Failed to reconstruct private key from bytes");
    }
    privateKey.reset(rawKey);
}

std::vector<uint8_t> Wallet::GetPrivateKeyBytes() const {
    BIGNUM* rawBN = nullptr;

    // get private key as BIGNUM
    if (EVP_PKEY_get_bn_param(privateKey.get(), OSSL_PKEY_PARAM_PRIV_KEY, &rawBN) <= 0) {
        throw std::runtime_error("Failed to get private key");
    }

    // wrap in RAII
    BN_ptr privKeyBN(rawBN, BN_free);

    // convert BIGNUM to bytes (32 bytes for secp256k1)
    std::vector<uint8_t> privKey(32);
    int numBytes = BN_bn2binpad(privKeyBN.get(), privKey.data(), 32);

    if (numBytes != 32) {
        throw std::runtime_error("Private key has wrong size");
    }

    return privKey;
}