#include "utils.h"
#include <sstream>
#include <iomanip>


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