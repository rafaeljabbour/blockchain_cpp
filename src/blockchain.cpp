#include "blockchain.h"

#include <leveldb/iterator.h>
#include <leveldb/write_batch.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>

#include "blockchainIterator.h"
#include "config.h"
#include "proofOfWork.h"
#include "transactionOutput.h"
#include "utils.h"
#include "wallet.h"

// RAII type alias for BIGNUM and BIGNUM context
using BN_CTX_ptr = std::unique_ptr<BN_CTX, decltype(&BN_CTX_free)>;
using BN_ptr = std::unique_ptr<BIGNUM, decltype(&BN_free)>;

bool Blockchain::DBExists() {
    leveldb::DB* rawDb = nullptr;
    leveldb::Options options;
    leveldb::Status status = leveldb::DB::Open(options, Config::GetBlocksPath(), &rawDb);
    std::unique_ptr<leveldb::DB> db(rawDb);

    return status.ok();
}

Blockchain::Blockchain() {
    if (!DBExists()) {
        throw std::runtime_error("No existing blockchain found. Create one first.");
    }

    leveldb::DB* rawDb = nullptr;
    leveldb::Options options;
    leveldb::Status status = leveldb::DB::Open(options, Config::GetBlocksPath(), &rawDb);

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

    std::vector<uint8_t> heightKey;
    heightKey.push_back('h');
    heightKey.insert(heightKey.end(), tip.begin(), tip.end());

    std::string heightString;
    status = db->Get(leveldb::ReadOptions(), ByteArrayToSlice(heightKey), &heightString);
    if (!status.ok()) {
        throw std::runtime_error("Error reading chain height: " + status.ToString());
    }
    std::vector<uint8_t> heightBytes(heightString.begin(), heightString.end());
    tipHeight = static_cast<int32_t>(ReadUint32(heightBytes, 0));
}

std::unique_ptr<Blockchain> Blockchain::CreateBlockchain(const std::string& address) {
    if (DBExists()) {
        throw std::runtime_error("Blockchain already exists.");
    }

    // ensure parent directory exists
    std::filesystem::create_directories(
        std::filesystem::path(Config::GetBlocksPath()).parent_path());

    leveldb::DB* rawDb = nullptr;
    leveldb::Options options;
    options.create_if_missing = true;

    leveldb::Status status = leveldb::DB::Open(options, Config::GetBlocksPath(), &rawDb);
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

    std::vector<uint8_t> genesisHeightKey;
    genesisHeightKey.push_back('h');
    genesisHeightKey.insert(genesisHeightKey.end(), genesisHash.begin(), genesisHash.end());

    std::vector<uint8_t> genesisHeightBytes;
    WriteUint32(genesisHeightBytes, 0);

    leveldb::WriteBatch batch;
    batch.Put(ByteArrayToSlice(key), ByteArrayToSlice(serialized));
    batch.Put("l", ByteArrayToSlice(genesisHash));
    batch.Put(ByteArrayToSlice(genesisHeightKey), ByteArrayToSlice(genesisHeightBytes));

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

    // compute the correct difficulty for the new block before running PoW
    int32_t nextBits = GetNextWorkRequired(GetChainHeight() + 1);

    Block newBlock(transactions, lastHash, nextBits);

    std::vector<uint8_t> blockKey;
    // prefix for the Block
    blockKey.push_back('b');
    std::vector<uint8_t> newHash = newBlock.GetHash();
    blockKey.insert(blockKey.end(), newHash.begin(), newHash.end());

    std::vector<uint8_t> serialized = newBlock.Serialize();

    std::vector<uint8_t> heightKey;
    heightKey.push_back('h');
    heightKey.insert(heightKey.end(), newHash.begin(), newHash.end());

    std::vector<uint8_t> newHeightBytes;
    WriteUint32(newHeightBytes, static_cast<uint32_t>(tipHeight + 1));

    leveldb::WriteBatch batch;
    batch.Put(ByteArrayToSlice(blockKey), ByteArrayToSlice(serialized));
    batch.Put("l", ByteArrayToSlice(newHash));
    batch.Put(ByteArrayToSlice(heightKey), ByteArrayToSlice(newHeightBytes));

    status = db->Write(leveldb::WriteOptions(), &batch);
    if (!status.ok()) {
        throw std::runtime_error("Error writing block: " + status.ToString());
    }

    tip = newHash;
    tipHeight++;
    return newBlock;
}

void Blockchain::AddBlock(const Block& block) {
    // verify the block links to our current tip
    if (block.GetPreviousHash() != tip) {
        throw std::runtime_error("Block's previous hash does not match current tip");
    }

    std::vector<uint8_t> blockHash = block.GetHash();

    // check if we already have this block
    std::vector<uint8_t> key;
    key.push_back('b');
    key.insert(key.end(), blockHash.begin(), blockHash.end());

    std::string existing;
    leveldb::Status status = db->Get(leveldb::ReadOptions(), ByteArrayToSlice(key), &existing);
    if (status.ok()) {
        return;
    }

    // store block and update tip atomically
    std::vector<uint8_t> serialized = block.Serialize();

    std::vector<uint8_t> heightKey;
    heightKey.push_back('h');
    heightKey.insert(heightKey.end(), blockHash.begin(), blockHash.end());

    std::vector<uint8_t> newHeightBytes;
    WriteUint32(newHeightBytes, static_cast<uint32_t>(tipHeight + 1));

    leveldb::WriteBatch batch;
    batch.Put(ByteArrayToSlice(key), ByteArrayToSlice(serialized));
    batch.Put("l", ByteArrayToSlice(blockHash));
    batch.Put(ByteArrayToSlice(heightKey), ByteArrayToSlice(newHeightBytes));

    status = db->Write(leveldb::WriteOptions(), &batch);
    if (!status.ok()) {
        throw std::runtime_error("Error writing block: " + status.ToString());
    }

    tip = blockHash;
    tipHeight++;
}

Block Blockchain::GetBlock(const std::vector<uint8_t>& hash) const {
    std::vector<uint8_t> key;
    key.push_back('b');
    key.insert(key.end(), hash.begin(), hash.end());

    std::string serializedBlock;
    leveldb::Status status =
        db->Get(leveldb::ReadOptions(), ByteArrayToSlice(key), &serializedBlock);
    if (!status.ok()) {
        throw std::runtime_error("Block not found");
    }

    std::vector<uint8_t> data(serializedBlock.begin(), serializedBlock.end());
    return Block::Deserialize(data);
}

std::vector<std::vector<uint8_t>> Blockchain::GetBlockHashesAfter(
    const std::vector<uint8_t>& afterHash) const {
    // walk from tip backwards collecting all hashes
    std::vector<std::vector<uint8_t>> allHashes;
    BlockchainIterator bci(tip, db.get());

    while (bci.hasNext()) {
        Block block = bci.Next();
        allHashes.push_back(block.GetHash());
    }

    // we now have newest first, reverse to get oldest first
    std::reverse(allHashes.begin(), allHashes.end());

    // find afterHash in the list
    for (size_t i = 0; i < allHashes.size(); i++) {
        if (allHashes[i] == afterHash) {
            // return everything after this position
            return std::vector<std::vector<uint8_t>>(allHashes.begin() + i + 1, allHashes.end());
        }
    }

    // if no hash is found then the peer is on a different chain, we can't help them sync
    return {};
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

                bool wasSpent = false;
                // check if the output was spent
                if (spentTXOs.count(txID)) {
                    for (int spentOutIdx : spentTXOs[txID]) {
                        if (spentOutIdx == static_cast<int>(outIdx)) {
                            wasSpent = true;
                            break;
                        }
                    }
                }
                if (!wasSpent) {
                    outs.outputs[static_cast<int>(outIdx)] = out;
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

    if (tx->GetVin().empty() || tx->GetVout().empty()) {
        return false;
    }

    std::map<std::string, Transaction> prevTXs;
    // collect all previous transactions being spent, the inputs this output is spending
    for (const auto& vin : tx->GetVin()) {
        Transaction prevTX = FindTransaction(vin.GetTxid());
        prevTXs.insert({ByteArrayToHexString(prevTX.GetID()), prevTX});
    }

    return tx->Verify(prevTXs);
}

bool Blockchain::VerifyTransaction(const Transaction* tx,
                                   const std::map<std::string, Transaction>& blockCtx) {
    if (tx->IsCoinbase()) {
        return true;
    }

    if (tx->GetVin().empty() || tx->GetVout().empty()) {
        return false;
    }

    std::map<std::string, Transaction> prevTXs;

    // collect all previous transactions being spent, the inputs this output is spending
    for (const auto& vin : tx->GetVin()) {
        std::string txidHex = ByteArrayToHexString(vin.GetTxid());

        // check for intra block spending first
        auto ctxIt = blockCtx.find(txidHex);
        if (ctxIt != blockCtx.end()) {
            prevTXs.insert({txidHex, ctxIt->second});
        } else {
            Transaction prevTX = FindTransaction(vin.GetTxid());
            prevTXs.insert({ByteArrayToHexString(prevTX.GetID()), prevTX});
        }
    }

    return tx->Verify(prevTXs);
}

BlockchainIterator Blockchain::Iterator() const { return BlockchainIterator(tip, db.get()); }

int32_t Blockchain::GetChainHeight() const { return tipHeight; }

int32_t Blockchain::GetBlockHeight(const std::vector<uint8_t>& hash) const {
    std::vector<uint8_t> heightKey;
    heightKey.push_back('h');
    heightKey.insert(heightKey.end(), hash.begin(), hash.end());

    std::string heightString;
    leveldb::Status status =
        db->Get(leveldb::ReadOptions(), ByteArrayToSlice(heightKey), &heightString);
    if (!status.ok()) {
        return -1;
    }

    std::vector<uint8_t> heightBytes(heightString.begin(), heightString.end());
    return static_cast<int32_t>(ReadUint32(heightBytes, 0));
}

int32_t Blockchain::GetNextWorkRequired(int32_t nextBlockHeight) const {
    // during genesis creation path
    if (tip.empty() || tip == std::vector<uint8_t>(32, 0)) {
        return INITIAL_BITS;
    }

    Block tipBlock = GetBlock(tip);

    // after some time we need to adjust the difficulty of the next blocks
    if (nextBlockHeight % RETARGET_INTERVAL != 0) {
        return tipBlock.GetBits();
    }

    // walk back RETARGET_INTERVAL − 1 steps to find the anchor block
    std::vector<uint8_t> anchorHash = tip;
    for (int32_t i = 0; i < RETARGET_INTERVAL - 1; ++i) {
        Block b = GetBlock(anchorHash);
        anchorHash = b.GetPreviousHash();
        // a redundancy check to ensure no issues
        if (anchorHash == std::vector<uint8_t>(32, 0)) {
            return tipBlock.GetBits();
        }
    }
    Block anchorBlock = GetBlock(anchorHash);

    int64_t actualTimespan = tipBlock.GetTimestamp() - anchorBlock.GetTimestamp();

    // 4x adjustment cap to prevent any extreme changes
    actualTimespan = std::max(actualTimespan, TARGET_TIMESPAN / 4);
    actualTimespan = std::min(actualTimespan, TARGET_TIMESPAN * 4);

    int32_t oldBits = tipBlock.GetBits();

    // oldTarget = 1 << (256 − oldBits)
    // newTarget = oldTarget × actualTimespan / TARGET_TIMESPAN
    BN_CTX_ptr ctx(BN_CTX_new(), BN_CTX_free);
    BN_ptr oldTarget(BN_new(), BN_free);
    BN_ptr newTarget(BN_new(), BN_free);
    BN_ptr bnActual(BN_new(), BN_free);
    BN_ptr bnExpected(BN_new(), BN_free);

    if (!ctx || !oldTarget || !newTarget || !bnActual || !bnExpected) {
        throw std::runtime_error("Failed to allocate BIGNUM resources during retarget");
    }

    BN_one(oldTarget.get());
    BN_lshift(oldTarget.get(), oldTarget.get(), 256 - oldBits);

    BN_set_word(bnActual.get(), static_cast<BN_ULONG>(actualTimespan));
    BN_set_word(bnExpected.get(), static_cast<BN_ULONG>(TARGET_TIMESPAN));

    // newTarget = (old target × actual time) / expected time
    BN_mul(newTarget.get(), oldTarget.get(), bnActual.get(), ctx.get());
    BN_div(newTarget.get(), nullptr, newTarget.get(), bnExpected.get(), ctx.get());

    // convert BIGNUM back to bits
    int32_t newBits = 257 - static_cast<int32_t>(BN_num_bits(newTarget.get()));
    newBits = std::max(MIN_BITS, std::min(MAX_BITS, newBits));

    std::cout << "[blockchain] Retarget at height " << nextBlockHeight << ": bits " << oldBits
              << " -> " << newBits << "  (actual=" << actualTimespan << "s"
              << ", expected=" << TARGET_TIMESPAN << "s)" << std::endl;

    return newBits;
}