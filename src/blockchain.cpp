#include "blockchain.h"

Blockchain::Blockchain() {
    blocks.push_back(Block("Genesis Block", std::vector<uint8_t>()));
}

void Blockchain::AddBlock(const std::string &data) {
    blocks.push_back(Block(data, blocks.back().GetHash()));
}