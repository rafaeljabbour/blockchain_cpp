#ifndef BASE58_H
#define BASE58_H

#include <cstdint>
#include <string>
#include <vector>

std::vector<uint8_t> Base58Encode(const std::vector<uint8_t>& input);
std::vector<uint8_t> Base58Decode(const std::vector<uint8_t>& input);

std::string Base58EncodeStr(const std::vector<uint8_t>& input);
std::vector<uint8_t> Base58DecodeStr(const std::string& input);

#endif