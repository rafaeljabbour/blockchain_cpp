#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <cstdint>
#include <string>
#include <vector>

#include "block.h"

class Blockchain {
  private:
    std::vector<Block> blocks; // vector of blocks

  public:
    Blockchain();
    void AddBlock(const std::string &data);

    const std::vector<Block>& GetBlocks() const { return blocks; }

};

#endif
