#include "utils.h"

#include <openssl/evp.h>

#include <iomanip>
#include <memory>
#include <sstream>

std::string IntToHexString(int64_t num) {
    std::stringstream ss;
    ss << std::hex << num;
    return ss.str();
}

std::string ByteArrayToHexString(const std::vector<uint8_t>& bytes) {
    std::stringstream ss;
    for (uint8_t b : bytes) {
        ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
    }
    return ss.str();
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
    // RAII wrapper used to create context window to track hashing state
    auto ctx =
        std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!ctx) {
        throw std::runtime_error("Failed to create SHA256 context");
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen = 0;

    // (init sha256, feed data into hash, complete and write has resul)
    if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) <= 0 ||
        EVP_DigestUpdate(ctx.get(), data.data(), data.size()) <= 0 ||
        EVP_DigestFinal_ex(ctx.get(), hash, &hashLen) <= 0) {
        throw std::runtime_error("SHA256 hashing failed");
    }

    return std::vector<uint8_t>(hash, hash + hashLen);
}

std::vector<uint8_t> RIPEMD160Hash(const std::vector<uint8_t>& data) {
    // RAII wrapper used to create context window to track hashing state
    auto ctx =
        std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!ctx) {
        throw std::runtime_error("Failed to create RIPEMD160 context");
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen = 0;

    // (init ripemd160, feed data into hash, complete and write has resul)
    if (EVP_DigestInit_ex(ctx.get(), EVP_ripemd160(), nullptr) <= 0 ||
        EVP_DigestUpdate(ctx.get(), data.data(), data.size()) <= 0 ||
        EVP_DigestFinal_ex(ctx.get(), hash, &hashLen) <= 0) {
        throw std::runtime_error("RIPEMD160 hashing failed");
    }

    return std::vector<uint8_t>(hash, hash + hashLen);
}

void WriteUint32(std::vector<uint8_t>& buf, uint32_t value) {
    buf.push_back(value & 0xFF);
    buf.push_back((value >> 8) & 0xFF);
    buf.push_back((value >> 16) & 0xFF);
    buf.push_back((value >> 24) & 0xFF);
}

void WriteUint64(std::vector<uint8_t>& buf, uint64_t value) {
    for (int i = 0; i < 8; i++) {
        buf.push_back((value >> (8 * i)) & 0xFF);
    }
}

uint32_t ReadUint32(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 4 > data.size()) {
        throw std::runtime_error("Data truncated: expected 4 bytes at offset " +
                                 std::to_string(offset));
    }
    return static_cast<uint32_t>(data[offset]) | (static_cast<uint32_t>(data[offset + 1]) << 8) |
           (static_cast<uint32_t>(data[offset + 2]) << 16) |
           (static_cast<uint32_t>(data[offset + 3]) << 24);
}

uint64_t ReadUint64(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 8 > data.size()) {
        throw std::runtime_error("Data truncated: expected 8 bytes at offset " +
                                 std::to_string(offset));
    }
    uint64_t value = 0;
    for (int i = 0; i < 8; i++) {
        value |= (static_cast<uint64_t>(data[offset + i]) << (8 * i));
    }
    return value;
}