#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include <cstdint>
#include <string>
#include <vector>

// hex string conversions
std::string ByteArrayToHexString(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> HexStringToByteArray(const std::string& hex);
std::string IntToHexString(int64_t num);

// integer to byte conversions
std::vector<uint8_t> IntToHexByteArray(int64_t num);
std::vector<uint8_t> StringToBytes(const std::string& str);
std::string BytesToString(const std::vector<uint8_t>& bytes);

// fixed width binary input/output
void WriteUint32(std::vector<uint8_t>& buf, uint32_t value);
void WriteUint64(std::vector<uint8_t>& buf, uint64_t value);
uint32_t ReadUint32(const std::vector<uint8_t>& data, size_t offset);
uint64_t ReadUint64(const std::vector<uint8_t>& data, size_t offset);

// utility
void ReverseBytes(std::vector<uint8_t>& data);

#endif