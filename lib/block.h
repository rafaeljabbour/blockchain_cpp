#ifndef BLOCK_H
#define BLOCK_H

#include <cstdint>
#include <string>
#include <vector>

class Block {
  private:
    int64_t timestamp; // when block is created
    std::vector<uint8_t> data;
    std::vector<uint8_t> previousHash; // Stores hash of previous block
    std::vector<uint8_t> hash;
    int32_t nonce; // counter to try different hashes in pow

  public:
    Block(const std::string &data, const std::vector<uint8_t> &previousHash);
    Block() = default;

    int64_t GetTimestamp() const { return timestamp; }
    const std::vector<uint8_t>& GetData() const { return data; }
    const std::vector<uint8_t>& GetPreviousHash() const { return previousHash; }
    const std::vector<uint8_t>& GetHash() const { return hash; }
    int32_t GetNonce() const { return nonce; }

    std::vector<uint8_t> Serialize() const;
    static Block Deserialize(const std::vector<uint8_t> &serialized);
};

#endif
