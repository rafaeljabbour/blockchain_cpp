#include "blockchain.h"

#include <leveldb/iterator.h>
#include <leveldb/write_batch.h>  // for atomic updates

#include <iostream>

#include "blockchainIterator.h"
#include "utils.h"

bool Blockchain::DBExists() {
    leveldb::DB* db;
    leveldb::Options options;
    leveldb::Status status = leveldb::DB::Open(options, DB_FILE, &db);

    if (status.ok()) {
        delete db;
        return true;
    }
    return false;
}

Blockchain::Blockchain() {
    if (!DBExists()) {
        std::cerr << "No existing blockchain found. Create one first." << std::endl;
        exit(1);
    }

    leveldb::Options options;
    leveldb::Status status = leveldb::DB::Open(options, DB_FILE, &db);

    if (!status.ok()) {
        std::cerr << "Error opening database: " << status.ToString() << std::endl;
        exit(1);
    }

    std::string tipString;
    status = db->Get(leveldb::ReadOptions(), "l", &tipString);
    if (status.ok()) {
        tip = std::vector<uint8_t>(tipString.begin(), tipString.end());
    } else {
        std::cerr << "Error reading tip: " << status.ToString() << std::endl;
        exit(1);
    }
}

std::unique_ptr<Blockchain> Blockchain::CreateBlockchain(const std::string& address) {
    if (DBExists()) {
        std::cerr << "DB already exists." << std::endl;
        exit(1);
    }

    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;

    leveldb::Status status = leveldb::DB::Open(options, DB_FILE, &db);
    if (!status.ok()) {
        std::cerr << "Error creating database: " << status.ToString() << std::endl;
        exit(1);
    }

    Transaction cbtx = Transaction::NewCoinbaseTX(address, GENESIS_COINBASE_DATA);
    Block genesis = Block::NewGenesisBlock(cbtx);

    std::vector<uint8_t> genesisHash = genesis.GetHash();
    std::vector<uint8_t> serialized = genesis.Serialize();

    leveldb::WriteBatch batch;

    batch.Put(ByteArrayToSlice(genesisHash), ByteArrayToSlice(serialized));
    batch.Put("l", ByteArrayToSlice(genesisHash));

    status = db->Write(leveldb::WriteOptions(), &batch);
    if (!status.ok()) {
        std::cerr << "Error writing genesis block: " << status.ToString() << std::endl;
        exit(1);
    }
    delete db;

    std::cout << "Genesis block created for address: " << address << std::endl;

    return std::make_unique<Blockchain>();
}

Blockchain::~Blockchain() { delete db; }

void Blockchain::MineBlock(const std::vector<Transaction>& transactions) {
    std::string lastHashString;
    leveldb::Status status = db->Get(leveldb::ReadOptions(), "l", &lastHashString);

    if (!status.ok()) {
        std::cerr << "Error reading last hash: " << status.ToString() << std::endl;
        exit(1);
    }

    std::vector<uint8_t> lastHash(lastHashString.begin(), lastHashString.end());

    Block newBlock(transactions, lastHash);

    std::vector<uint8_t> newHash = newBlock.GetHash();
    std::vector<uint8_t> serialized = newBlock.Serialize();

    leveldb::WriteBatch batch;
    batch.Put(ByteArrayToSlice(newHash), ByteArrayToSlice(serialized));
    batch.Put("l", ByteArrayToSlice(newHash));

    status = db->Write(leveldb::WriteOptions(), &batch);
    if (!status.ok()) {
        std::cerr << "Error writing block: " << status.ToString() << std::endl;
        exit(1);
    }

    tip = newHash;
}

std::vector<Transaction> Blockchain::FindUnspentTransactions(const std::string& address) {
    std::vector<Transaction> unspentTXs;
    std::map<std::string, std::vector<int>> spentTXOs;
    BlockchainIterator bci = Iterator();

    while (bci.hasNext()) {
        Block block = bci.Next();

        for (const Transaction& tx : block.GetTransactions()) {
            std::string txID = ByteArrayToHexString(tx.GetID());

            for (size_t outIdx = 0; outIdx < tx.GetVout().size(); outIdx++) {
                const TransactionOutput& out = tx.GetVout()[outIdx];

                if (spentTXOs.count(txID)) {
                    bool wasSpent = false;
                    for (int spentOut : spentTXOs[txID]) {
                        if (spentOut == static_cast<int>(outIdx)) {
                            wasSpent = true;
                            break;
                        }
                    }
                    if (wasSpent) continue;
                }
                if (out.CanBeUnlockedWith(address)) {
                    unspentTXs.push_back(tx);
                }
            }
            // Gather spent outputs
            if (!tx.IsCoinbase()) {
                for (const TransactionInput& in : tx.GetVin()) {
                    if (in.CanUnlockOutputWith(address)) {
                        std::string inTxID = ByteArrayToHexString(in.GetTxid());
                        spentTXOs[inTxID].push_back(in.GetVout());
                    }
                }
            }
        }
    }

    return unspentTXs;
}

std::vector<TransactionOutput> Blockchain::FindUTXO(const std::string& address) {
    std::vector<TransactionOutput> UTXOs;
    std::vector<Transaction> unspentTransactions = FindUnspentTransactions(address);

    for (const Transaction& tx : unspentTransactions) {
        for (const TransactionOutput& out : tx.GetVout()) {
            if (out.CanBeUnlockedWith(address)) {
                UTXOs.push_back(out);
            }
        }
    }

    return UTXOs;
}

std::pair<int, std::map<std::string, std::vector<int>>> Blockchain::FindSpendableOutputs(
    const std::string& address, int amount) {
    std::map<std::string, std::vector<int>> unspentOutputs;
    std::vector<Transaction> unspentTXs = FindUnspentTransactions(address);
    int accumulated = 0;

    for (const Transaction& tx : unspentTXs) {
        std::string txID = ByteArrayToHexString(tx.GetID());

        for (size_t outIdx = 0; outIdx < tx.GetVout().size(); outIdx++) {
            const TransactionOutput& out = tx.GetVout()[outIdx];

            if (out.CanBeUnlockedWith(address) && accumulated < amount) {
                accumulated += out.GetValue();
                unspentOutputs[txID].push_back(outIdx);

                if (accumulated >= amount) {
                    goto Done;  // Break out of nested loops
                }
            }
        }
    }

Done:
    return {accumulated, unspentOutputs};
}

BlockchainIterator Blockchain::Iterator() { return BlockchainIterator(tip, db); }