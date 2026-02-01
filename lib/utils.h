#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include <string>
#include <vector>

std::string IntToHexString(int64_t num);
std::string ByteArrayToHexString(const std::vector<uint8_t>& bytes);
std::string ByteArrayToString(const std::vector<uint8_t>& bytes);

#endif