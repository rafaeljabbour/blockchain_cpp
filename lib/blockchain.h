#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <leveldb/db.h>

#include <cstdint>
#include <string>
#include <vector>

#include "block.h"
#include "blockchainIterator.h"

class Blockchain {
    private:
        std::vector<uint8_t> tip;  // hash of the last block
        leveldb::DB* db;           // leveldb for storing blocks (persistency)

    public:
        Blockchain();
        ~Blockchain();
        void AddBlock(const std::string& data);

        BlockchainIterator Iterator();
};

#endif