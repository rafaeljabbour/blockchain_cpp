#ifndef BLOCKCHAINITERATOR_H
#define BLOCKCHAINITERATOR_H

#include <cstdint>
#include <string>
#include <vector>

#include <leveldb/db.h>
#include "block.h"

class BlockchainIterator{
    private:
        std::vector<uint8_t> currentHash;
        leveldb::DB* db;

    public:
        BlockchainIterator(std::vector<uint8_t> tip, leveldb::DB* db);
        Block Next();
        bool hasNext() const;
};


#endif