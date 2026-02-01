#include "block.h"
#include <ctime>
#include <openssl/sha.h>
#include "proofOfWork.h"

Block::Block(const std::string &data, const std::vector<uint8_t> &previousHash){
  timestamp = std::time(nullptr);
  this->data = std::vector<uint8_t>(data.begin(), data.end());  
  this->previousHash = previousHash;
  
  ProofOfWork proofOfWork(this);
  std::pair<int32_t, std::vector<uint8_t>> powResult = proofOfWork.Run();
  nonce = powResult.first;
  hash = powResult.second;
}
