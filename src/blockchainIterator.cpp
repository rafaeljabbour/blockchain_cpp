#include "blockchainIterator.h"

#include <vector>

#include "block.h"
#include "utils.h"

BlockchainIterator::BlockchainIterator(std::vector<uint8_t> tip, leveldb::DB* db)
    : currentHash(std::move(tip)), db(db) {}

Block BlockchainIterator::Next() {
    std::string serializedBlock;

    leveldb::Status status =
        db->Get(leveldb::ReadOptions(), ByteArrayToSlice(currentHash), &serializedBlock);
    if (!status.ok()) {
        throw std::runtime_error("Failed to read block from database: " + status.ToString());
    }

    std::vector<uint8_t> data(serializedBlock.begin(), serializedBlock.end());
    Block block = Block::Deserialize(data);

    currentHash = block.GetPreviousHash();
    return block;
}

bool BlockchainIterator::hasNext() const {
    for (uint8_t byte : currentHash) {
        if (byte != 0) return true;
    }
    return false;
}