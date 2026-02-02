#ifndef UTILS_H
#define UTILS_H

#include <leveldb/slice.h>

#include <cstdint>
#include <string>
#include <vector>

std::string IntToHexString(int64_t num);
std::string ByteArrayToHexString(const std::vector<uint8_t>& bytes);
std::string ByteArrayToString(const std::vector<uint8_t>& bytes);
leveldb::Slice ByteArrayToSlice(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> IntToHexByteArray(int64_t num);
std::vector<uint8_t> HexStringToByteArray(const std::string& hex);

#endif