#include "utils.h"

#include <openssl/evp.h>

#include <iomanip>
#include <iostream>
#include <sstream>

std::string IntToHexString(int64_t num) {
    std::stringstream ss;
    ss << std::hex << num;
    return ss.str();
}

std::string ByteArrayToHexString(const std::vector<uint8_t>& bytes) {
    std::stringstream ss;
    for (uint8_t b : bytes) {
        ss << std::hex << std::setfill('0') << std::setw(2) << (int)b;
    }
    return ss.str();
}

std::string ByteArrayToString(const std::vector<uint8_t>& bytes) {
    return std::string(bytes.begin(), bytes.end());
}

leveldb::Slice ByteArrayToSlice(const std::vector<uint8_t>& bytes) {
    return leveldb::Slice(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

std::vector<uint8_t> IntToHexByteArray(int64_t num) {
    std::vector<uint8_t> bytes(8);

    for (int i = 0; i < 8; i++) {
        bytes[7 - i] = (num >> (8 * i)) & 0xFF;
    }

    return bytes;
}

std::vector<uint8_t> HexStringToByteArray(const std::string& hex) {
    std::vector<uint8_t> bytes;

    std::string paddedHex = hex;
    if (paddedHex.length() % 2 != 0) {
        paddedHex = "0" + paddedHex;
    }

    for (size_t i = 0; i < paddedHex.length(); i += 2) {
        std::string byteString = paddedHex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(strtol(byteString.c_str(), nullptr, 16));
        bytes.push_back(byte);
    }

    return bytes;
}

void ReverseBytes(std::vector<uint8_t>& data) { std::reverse(data.begin(), data.end()); }

std::vector<uint8_t> StringToBytes(const std::string& str) {
    return std::vector<uint8_t>(str.begin(), str.end());
}

std::string BytesToString(const std::vector<uint8_t>& bytes) {
    return std::string(bytes.begin(), bytes.end());
}

std::vector<uint8_t> SHA256Hash(const std::vector<uint8_t>& data) {
    // Create context window to track hashing state
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        std::cerr << "Error: Failed to create SHA256 context" << std::endl;
        exit(1);
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen = 0;

    // (init sha256, feed data into hash, complete and write has resul)
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) <= 0 ||
        EVP_DigestUpdate(ctx, data.data(), data.size()) <= 0 ||
        EVP_DigestFinal_ex(ctx, hash, &hashLen) <= 0) {
        std::cerr << "Error: SHA256 hashing failed" << std::endl;
        EVP_MD_CTX_free(ctx);
        exit(1);
    }

    EVP_MD_CTX_free(ctx);
    return std::vector<uint8_t>(hash, hash + hashLen);
}

std::vector<uint8_t> RIPEMD160Hash(const std::vector<uint8_t>& data) {
    // Create context window to track hashing state
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        std::cerr << "Error: Failed to create RIPEMD160 context" << std::endl;
        exit(1);
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen = 0;

    // (init ripemd160, feed data into hash, complete and write has resul)
    if (EVP_DigestInit_ex(ctx, EVP_ripemd160(), nullptr) <= 0 ||
        EVP_DigestUpdate(ctx, data.data(), data.size()) <= 0 ||
        EVP_DigestFinal_ex(ctx, hash, &hashLen) <= 0) {
        std::cerr << "Error: RIPEMD160 hashing failed" << std::endl;
        EVP_MD_CTX_free(ctx);
        exit(1);
    }

    EVP_MD_CTX_free(ctx);
    return std::vector<uint8_t>(hash, hash + hashLen);
}