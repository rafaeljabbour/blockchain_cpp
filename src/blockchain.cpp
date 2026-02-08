#include "blockchain.h"

#include <leveldb/iterator.h>
#include <leveldb/write_batch.h>  // for atomic updates

#include <iostream>

#include "blockchainIterator.h"
#include "utils.h"
#include "wallet.h"

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
        std::cerr << "Blockchain already exists." << std::endl;
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
    // verify all transactions before mining
    for (const Transaction& tx : transactions) {
        if (!VerifyTransaction(&tx)) {
            std::cerr << "ERROR: Invalid transaction" << std::endl;
            exit(1);
        }
    }

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

std::vector<Transaction> Blockchain::FindUnspentTransactions(
    const std::vector<uint8_t>& pubKeyHash) {
    std::vector<Transaction> unspentTXs;
    std::map<std::string, std::vector<int>> spentTXOs;
    BlockchainIterator bci = Iterator();

    while (bci.hasNext()) {
        Block block = bci.Next();

        for (const Transaction& tx : block.GetTransactions()) {
            std::string txID = ByteArrayToHexString(tx.GetID());

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
                    if (wasSpent) continue;
                }

                if (out.IsLockedWithKey(pubKeyHash)) {
                    unspentTXs.push_back(tx);
                }
            }

            // gather spent outputs
            if (!tx.IsCoinbase()) {
                for (const TransactionInput& in : tx.GetVin()) {
                    if (in.UsesKey(pubKeyHash)) {
                        std::string inTxID = ByteArrayToHexString(in.GetTxid());
                        spentTXOs[inTxID].push_back(in.GetVout());
                    }
                }
            }
        }
    }

    return unspentTXs;
}

std::vector<TransactionOutput> Blockchain::FindUTXO(const std::vector<uint8_t>& pubKeyHash) {
    std::vector<TransactionOutput> UTXOs;
    std::vector<Transaction> unspentTransactions = FindUnspentTransactions(pubKeyHash);

    for (const Transaction& tx : unspentTransactions) {
        for (const TransactionOutput& out : tx.GetVout()) {
            if (out.IsLockedWithKey(pubKeyHash)) {
                UTXOs.push_back(out);
            }
        }
    }

    return UTXOs;
}

std::pair<int, std::map<std::string, std::vector<int>>> Blockchain::FindSpendableOutputs(
    const std::vector<uint8_t>& pubKeyHash, int amount) {
    std::map<std::string, std::vector<int>> unspentOutputs;
    std::vector<Transaction> unspentTXs = FindUnspentTransactions(pubKeyHash);
    int accumulated = 0;

    for (const Transaction& tx : unspentTXs) {
        std::string txID = ByteArrayToHexString(tx.GetID());

        for (size_t outIdx = 0; outIdx < tx.GetVout().size(); outIdx++) {
            const TransactionOutput& out = tx.GetVout()[outIdx];

            if (out.IsLockedWithKey(pubKeyHash) && accumulated < amount) {
                accumulated += out.GetValue();
                unspentOutputs[txID].push_back(outIdx);

                if (accumulated >= amount) {
                    // to break out of the nested loops
                    goto Done;
                }
            }
        }
    }

Done:
    return {accumulated, unspentOutputs};
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

    std::cerr << "Error: Transaction not found" << std::endl;
    exit(1);
}

void Blockchain::SignTransaction(Transaction* tx, Wallet* wallet) {
    std::map<std::string, Transaction> prevTXs;

    // collect all previous transactions being spent, the inputs this output is spending
    for (const auto& vin : tx->GetVin()) {
        Transaction prevTX = FindTransaction(vin.GetTxid());
        prevTXs[ByteArrayToHexString(prevTX.GetID())] = prevTX;
    }

    tx->Sign(wallet->privateKey, prevTXs);
}

bool Blockchain::VerifyTransaction(const Transaction* tx) {
    if (tx->IsCoinbase()) {
        return true;
    }

    std::map<std::string, Transaction> prevTXs;

    // collect all previous transactions being spent, the inputs this output is spending
    for (const auto& vin : tx->GetVin()) {
        Transaction prevTX = FindTransaction(vin.GetTxid());
        prevTXs[ByteArrayToHexString(prevTX.GetID())] = prevTX;
    }

    return tx->Verify(prevTXs);
}

BlockchainIterator Blockchain::Iterator() { return BlockchainIterator(tip, db); }