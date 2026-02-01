#include "blockchain.h"
#include "utils.h"

#include <iostream>
#include <leveldb/write_batch.h> // for atomic updates

Blockchain::Blockchain() {
    leveldb::Options options;
    options.create_if_missing = true; 

    leveldb::Status status = leveldb::DB::Open(options, "./tmp/blocks", &db);
    assert(status.ok());

    std::string tipString;
    status = db->Get(leveldb::ReadOptions(), "l", &tipString);
    if (status.ok()) {
        tip = std::vector<uint8_t>(tipString.begin(), tipString.end());
    }
    else if (status.IsNotFound()) {
        Block genesis("Genesis Block", std::vector<uint8_t>());

        std::vector<uint8_t> genesisHash = genesis.GetHash();
        std::vector<uint8_t> serialized = genesis.Serialize();

        leveldb::WriteBatch batch;

        batch.Put(ByteArrayToString(genesisHash), ByteArrayToString(serialized));
        batch.Put("l", ByteArrayToString(genesisHash));

        status = db->Write(leveldb::WriteOptions(), &batch);
        assert(status.ok());
        tip = genesisHash;
    }
    else {
        std::cerr << "Error reading last hash: " << status.ToString() << std::endl;
        assert(false);
    }
}

void Blockchain::AddBlock(const std::string &data) {
    std::string tipString;
    leveldb::Status s = db->Get(leveldb::ReadOptions(), "l", &tipString);
    assert(s.ok());

    std::vector<uint8_t> lastHash(tipString.begin(), tipString.end());

    Block newBlock(data, lastHash);

    std::vector<uint8_t> newHash = newBlock.GetHash();
    std::vector<uint8_t> serialized = newBlock.Serialize();

    leveldb::WriteBatch batch;

    batch.Put(ByteArrayToString(newHash), ByteArrayToString(serialized));
    batch.Put("l", ByteArrayToString(newHash));

    leveldb::Status status = db->Write(leveldb::WriteOptions(), &batch);
    assert(status.ok());
    tip = newHash;
}