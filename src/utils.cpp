#include "utils.h"

#include <iomanip>
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