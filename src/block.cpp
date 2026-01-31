#include "block.h"
#include <ctime>
#include <openssl/sha.h>

Block::Block(const std::string &data, const std::vector<uint8_t> &previousHash){
  timestamp = std::time(nullptr);
  this->data = std::vector<uint8_t>(data.begin(), data.end());  
  this->previousHash = previousHash;
  SetHash();
}

void Block::SetHash() {
  std::string timestampString = std::to_string(timestamp);

  std::vector<uint8_t> header;
  header.insert(header.end(), previousHash.begin(), previousHash.end());
  header.insert(header.end(), data.begin(), data.end());
  header.insert(header.end(), timestampString.begin(), timestampString.end());

  uint8_t hashResult[SHA256_DIGEST_LENGTH];

  SHA256(header.data(), header.size(), hashResult);
  hash.assign(hashResult, hashResult + SHA256_DIGEST_LENGTH);
}
