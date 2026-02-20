#include "serialization.h"

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <stdexcept>

std::string ByteArrayToHexString(const std::vector<uint8_t>& bytes) {
    std::ostringstream ss;
    for (uint8_t b : bytes) {
        ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
    }
    return ss.str();
}

std::vector<uint8_t> HexStringToByteArray(const std::string& hex) {
    std::string padded = hex;
    if (padded.size() % 2 != 0) {
        padded.insert(padded.begin(), '0');
    }

    std::vector<uint8_t> bytes;
    bytes.reserve(padded.size() / 2);
    for (size_t i = 0; i < padded.size(); i += 2) {
        bytes.push_back(
            static_cast<uint8_t>(std::strtol(padded.substr(i, 2).c_str(), nullptr, 16)));
    }
    return bytes;
}

std::string IntToHexString(int64_t num) {
    std::ostringstream ss;
    ss << std::hex << num;
    return ss.str();
}

std::vector<uint8_t> IntToHexByteArray(int64_t num) {
    std::vector<uint8_t> bytes(8);
    for (int i = 0; i < 8; i++) {
        bytes[7 - i] = static_cast<uint8_t>((num >> (8 * i)) & 0xFF);
    }
    return bytes;
}

std::vector<uint8_t> StringToBytes(const std::string& str) { return {str.begin(), str.end()}; }

std::string BytesToString(const std::vector<uint8_t>& bytes) {
    return {bytes.begin(), bytes.end()};
}

void ReverseBytes(std::vector<uint8_t>& data) { std::reverse(data.begin(), data.end()); }

void WriteUint32(std::vector<uint8_t>& buf, uint32_t value) {
    buf.push_back(value & 0xFF);
    buf.push_back((value >> 8) & 0xFF);
    buf.push_back((value >> 16) & 0xFF);
    buf.push_back((value >> 24) & 0xFF);
}

void WriteUint64(std::vector<uint8_t>& buf, uint64_t value) {
    for (int i = 0; i < 8; i++) {
        buf.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xFF));
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
