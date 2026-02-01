#include "blockchainIterator.h"
#include "block.h"
#include "utils.h"
#include <cerrno>
#include <vector>
#include <iostream>

Block BlockchainIterator::Next(){
    std::string serializedBlock;

    leveldb::Status status = db->Get(leveldb::ReadOptions(), ByteArrayToSlice(currentHash), &serializedBlock);
    if (!status.ok()) {
        if (!status.ok()) {
            std::cerr << "Error reading block: " << status.ToString() << std::endl;
            throw std::runtime_error("Failed to read block from database");
        }    }

    std::vector<uint8_t> data(serializedBlock.begin(), serializedBlock.end());
    Block block = Block::Deserialize(data);

    currentHash = block.GetPreviousHash();
    return block;
} 

bool BlockchainIterator::hasNext() const{
    for(uint8_t byte: currentHash){
        if(byte != 0) return true;
    }
    return false;
}
