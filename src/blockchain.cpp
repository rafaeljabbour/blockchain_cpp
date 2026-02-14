#include "blockchain.h"

#include <leveldb/iterator.h>
#include <leveldb/write_batch.h>

#include <filesystem>
#include <iostream>

#include "blockchainIterator.h"
#include "transactionOutput.h"
#include "utils.h"
#include "wallet.h"

bool Blockchain::DBExists() {
    leveldb::DB* rawDb = nullptr;
    leveldb::Options options;
    leveldb::Status status = leveldb::DB::Open(options, DB_FILE, &rawDb);
    std::unique_ptr<leveldb::DB> db(rawDb);

    return status.ok();
}

Blockchain::Blockchain() {
    if (!DBExists()) {
        throw std::runtime_error("No existing blockchain found. Create one first.");
    }

    leveldb::DB* rawDb = nullptr;
    leveldb::Options options;
    leveldb::Status status = leveldb::DB::Open(options, DB_FILE, &rawDb);

    if (!status.ok()) {
        throw std::runtime_error("Error opening database: " + status.ToString());
    }

    db.reset(rawDb);

    std::string tipString;
    status = db->Get(leveldb::ReadOptions(), "l", &tipString);
    if (!status.ok()) {
        throw std::runtime_error("Error reading tip: " + status.ToString());
    }

    tip = std::vector<uint8_t>(tipString.begin(), tipString.end());
}

std::unique_ptr<Blockchain> Blockchain::CreateBlockchain(const std::string& address) {
    if (DBExists()) {
        throw std::runtime_error("Blockchain already exists.");
    }

    // ensure parent directory exists
    std::filesystem::create_directories(std::filesystem::path(DB_FILE).parent_path());

    leveldb::DB* rawDb = nullptr;
    leveldb::Options options;
    options.create_if_missing = true;

    leveldb::Status status = leveldb::DB::Open(options, DB_FILE, &rawDb);
    if (!status.ok()) {
        throw std::runtime_error("Error creating database: " + status.ToString());
    }

    // RAII wrap
    std::unique_ptr<leveldb::DB> tempDb(rawDb);

    Transaction cbtx = Transaction::NewCoinbaseTX(address, GENESIS_COINBASE_DATA);
    Block genesis = Block::NewGenesisBlock(cbtx);

    std::vector<uint8_t> genesisHash = genesis.GetHash();
    std::vector<uint8_t> serialized = genesis.Serialize();

    std::vector<uint8_t> key;
    // prefix for the Block
    key.push_back('b');
    key.insert(key.end(), genesisHash.begin(), genesisHash.end());

    leveldb::WriteBatch batch;
    batch.Put(ByteArrayToSlice(key), ByteArrayToSlice(serialized));
    batch.Put("l", ByteArrayToSlice(genesisHash));

    status = tempDb->Write(leveldb::WriteOptions(), &batch);
    if (!status.ok()) {
        throw std::runtime_error("Error writing genesis block: " + status.ToString());
    }

    // release the temp DB before constructing Blockchain, the blockchain opens it's own
    tempDb.reset();

    std::cout << "Genesis block created for address: " << address << std::endl;

    return std::make_unique<Blockchain>();
}

Block Blockchain::MineBlock(const std::vector<Transaction>& transactions) {
    // verify all transactions before mining
    for (const Transaction& tx : transactions) {
        if (!VerifyTransaction(&tx)) {
            throw std::runtime_error("Invalid transaction");
        }
    }

    std::string lastHashString;
    leveldb::Status status = db->Get(leveldb::ReadOptions(), "l", &lastHashString);

    if (!status.ok()) {
        throw std::runtime_error("Error reading last hash: " + status.ToString());
    }

    std::vector<uint8_t> lastHash(lastHashString.begin(), lastHashString.end());

    Block newBlock(transactions, lastHash);

    std::vector<uint8_t> blockKey;
    // prefix for the Block
    blockKey.push_back('b');
    std::vector<uint8_t> newHash = newBlock.GetHash();
    blockKey.insert(blockKey.end(), newHash.begin(), newHash.end());

    std::vector<uint8_t> serialized = newBlock.Serialize();

    leveldb::WriteBatch batch;
    batch.Put(ByteArrayToSlice(blockKey), ByteArrayToSlice(serialized));
    batch.Put("l", ByteArrayToSlice(newHash));

    status = db->Write(leveldb::WriteOptions(), &batch);
    if (!status.ok()) {
        throw std::runtime_error("Error writing block: " + status.ToString());
    }

    tip = newHash;
    return newBlock;
}

std::map<std::string, TXOutputs> Blockchain::FindUTXO() {
    std::map<std::string, TXOutputs> UTXO;
    std::map<std::string, std::vector<int>> spentTXOs;
    BlockchainIterator bci = Iterator();

    while (bci.hasNext()) {
        Block block = bci.Next();

        for (const Transaction& tx : block.GetTransactions()) {
            std::string txID = ByteArrayToHexString(tx.GetID());
            TXOutputs outs;

            for (size_t outIdx = 0; outIdx < tx.GetVout().size(); outIdx++) {
                const TransactionOutput& out = tx.GetVout()[outIdx];

                // check if the output was spent
                if (spentTXOs.count(txID)) {
                    bool wasSpent = false;
                    for (int spentOutIdx : spentTXOs[txID]) {
                        if (spentOutIdx == static_cast<int>(outIdx)) {
                            wasSpent = true;
                            break;
                        }
                    }
                    if (!wasSpent) {
                        outs.outputs.push_back(out);
                    }
                }
            }

            if (!outs.outputs.empty()) {
                UTXO[txID] = outs;
            }

            // gather spent outputs
            if (!tx.IsCoinbase()) {
                for (const TransactionInput& in : tx.GetVin()) {
                    std::string inputTxID = ByteArrayToHexString(in.GetTxid());
                    spentTXOs[inputTxID].push_back(in.GetVout());
                }
            }
        }
    }

    return UTXO;
}

Transaction Blockchain::FindTransaction(const std::vector<uint8_t>& ID) {
    BlockchainIterator bci = Iterator();

    while (bci.hasNext()) {
        Block block = bci.Next();

        for (const Transaction& tx : block.GetTransactions()) {
            if (tx.GetID() == ID) {
                return tx;
            }
        }
    }

    throw std::runtime_error("Transaction not found");
}

void Blockchain::SignTransaction(Transaction* tx, Wallet* wallet) {
    std::map<std::string, Transaction> prevTXs;

    // collect all previous transactions being spent, the inputs this output is spending
    for (const auto& vin : tx->GetVin()) {
        Transaction prevTX = FindTransaction(vin.GetTxid());
        prevTXs.insert({ByteArrayToHexString(prevTX.GetID()), prevTX});
    }

    tx->Sign(wallet->privateKey.get(), prevTXs);
}

bool Blockchain::VerifyTransaction(const Transaction* tx) {
    if (tx->IsCoinbase()) {
        return true;
    }

    std::map<std::string, Transaction> prevTXs;

    // collect all previous transactions being spent, the inputs this output is spending
    for (const auto& vin : tx->GetVin()) {
        Transaction prevTX = FindTransaction(vin.GetTxid());
        prevTXs.insert({ByteArrayToHexString(prevTX.GetID()), prevTX});
    }

    return tx->Verify(prevTXs);
}

BlockchainIterator Blockchain::Iterator() { return BlockchainIterator(tip, db.get()); }