#ifndef UTILS_H
#define UTILS_H

#include <leveldb/slice.h>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

std::string IntToHexString(int64_t num);
std::string ByteArrayToHexString(const std::vector<uint8_t>& bytes);
leveldb::Slice ByteArrayToSlice(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> IntToHexByteArray(int64_t num);
std::vector<uint8_t> HexStringToByteArray(const std::string& hex);
void ReverseBytes(std::vector<uint8_t>& data);
std::vector<uint8_t> StringToBytes(const std::string& str);
std::string BytesToString(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> SHA256Hash(const std::vector<uint8_t>& data);
std::vector<uint8_t> RIPEMD160Hash(const std::vector<uint8_t>& data);

// in little-endian byte order
void WriteUint32(std::vector<uint8_t>& buf, uint32_t value);
void WriteUint64(std::vector<uint8_t>& buf, uint64_t value);
uint32_t ReadUint32(const std::vector<uint8_t>& data, size_t offset);
uint64_t ReadUint64(const std::vector<uint8_t>& data, size_t offset);

#endif