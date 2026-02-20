#ifndef CRYPTO_H
#define CRYPTO_H

#include <cstdint>
#include <vector>

std::vector<uint8_t> SHA256Hash(const std::vector<uint8_t>& data);

std::vector<uint8_t> SHA256DoubleHash(const std::vector<uint8_t>& data);

std::vector<uint8_t> RIPEMD160Hash(const std::vector<uint8_t>& data);

#endif