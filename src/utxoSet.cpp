#include "utxoSet.h"

#include <leveldb/iterator.h>
#include <leveldb/write_batch.h>

#include <filesystem>
#include <memory>
#include <stdexcept>

#include "block.h"
#include "blockchain.h"
#include "config.h"
#include "utils.h"

UTXOSet::UTXOSet(Blockchain* bc) : blockchain(bc) {
    if (!bc) {
        throw std::invalid_argument("Blockchain pointer cannot be null");
    }

    // ensure parent directory exists
    std::filesystem::create_directories(std::filesystem::path(Config::GetUTXOPath()).parent_path());

    leveldb::DB* rawDb = nullptr;
    leveldb::Options options;
    options.create_if_missing = true;

    leveldb::Status status = leveldb::DB::Open(options, Config::GetUTXOPath(), &rawDb);
    if (!status.ok()) {
        throw std::runtime_error("Error opening UTXO database: " + status.ToString());
    }

    db.reset(rawDb);
}

std::pair<int64_t, std::map<std::string, std::vector<int>>> UTXOSet::FindSpendableOutputs(
    const std::vector<uint8_t>& pubKeyHash, int64_t amount) const {
    std::map<std::string, std::vector<int>> unspentOutputs;
    int64_t accumulated = 0;
    bool found = false;

    int32_t currentHeight = blockchain->GetChainHeight();

    std::unique_ptr<leveldb::Iterator> it(db->NewIterator(leveldb::ReadOptions()));

    for (it->SeekToFirst(); it->Valid() && !found; it->Next()) {
        std::string key = it->key().ToString();
        std::string value = it->value().ToString();

        std::string txID = ByteArrayToHexString(std::vector<uint8_t>(key.begin(), key.end()));

        std::vector<uint8_t> valueBytes(value.begin(), value.end());
        TXOutputs outs = TXOutputs::Deserialize(valueBytes);

        // can't spend unless coinbase is mature
        if (outs.isCoinbase) {
            int32_t depth = currentHeight - outs.blockHeight;
            if (depth < Consensus::COINBASE_MATURITY) {
                continue;
            }
        }

        for (const auto& [origIdx, out] : outs.outputs) {
            if (out.IsLockedWithKey(pubKeyHash) && accumulated < amount) {
                accumulated += out.GetValue();
                unspentOutputs[txID].push_back(origIdx);

                if (accumulated >= amount) {
                    found = true;
                    break;
                }
            }
        }
    }

    if (!it->status().ok()) {
        throw std::runtime_error("Error iterating UTXO set: " + it->status().ToString());
    }

    return {accumulated, unspentOutputs};
}

std::vector<TransactionOutput> UTXOSet::FindUTXO(const std::vector<uint8_t>& pubKeyHash) const {
    std::vector<TransactionOutput> UTXOs;

    int32_t currentHeight = blockchain->GetChainHeight();

    std::unique_ptr<leveldb::Iterator> it(db->NewIterator(leveldb::ReadOptions()));

    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        std::string key = it->key().ToString();
        std::string value = it->value().ToString();

        std::vector<uint8_t> valueBytes(value.begin(), value.end());
        TXOutputs outs = TXOutputs::Deserialize(valueBytes);

        // skip immature coinbase outputs
        if (outs.isCoinbase) {
            int32_t depth = currentHeight - outs.blockHeight;
            if (depth < Consensus::COINBASE_MATURITY) {
                continue;
            }
        }

        for (const auto& [origIdx, out] : outs.outputs) {
            if (out.IsLockedWithKey(pubKeyHash)) {
                UTXOs.push_back(out);
            }
        }
    }

    if (!it->status().ok()) {
        throw std::runtime_error("Error iterating UTXO set: " + it->status().ToString());
    }

    return UTXOs;
}

int UTXOSet::CountTransactions() const {
    int counter = 0;

    std::unique_ptr<leveldb::Iterator> it(db->NewIterator(leveldb::ReadOptions()));

    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        counter++;
    }

    if (!it->status().ok()) {
        throw std::runtime_error("Error iterating UTXO set: " + it->status().ToString());
    }

    return counter;
}

void UTXOSet::Reindex() {
    // wipe the entire UTXO database
    std::vector<std::string> keysToDelete;

    auto it = std::unique_ptr<leveldb::Iterator>(db->NewIterator(leveldb::ReadOptions()));
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        keysToDelete.push_back(it->key().ToString());
    }

    if (!it->status().ok()) {
        throw std::runtime_error("Error scanning UTXO database: " + it->status().ToString());
    }

    leveldb::WriteBatch deleteBatch;
    for (const auto& key : keysToDelete) {
        deleteBatch.Delete(key);
    }

    leveldb::Status status = db->Write(leveldb::WriteOptions(), &deleteBatch);
    if (!status.ok()) {
        throw std::runtime_error("Error clearing UTXO database: " + status.ToString());
    }

    // build new UTXO set from the blockchain
    std::map<std::string, TXOutputs> UTXO = blockchain->FindUTXO();

    // write the new UTXO set, where the keys are raw txid bytes
    leveldb::WriteBatch newBatch;

    for (const auto& [txID, outs] : UTXO) {
        std::vector<uint8_t> key = HexStringToByteArray(txID);
        std::vector<uint8_t> value = outs.Serialize();

        newBatch.Put(ByteArrayToSlice(key), ByteArrayToSlice(value));
    }

    status = db->Write(leveldb::WriteOptions(), &newBatch);
    if (!status.ok()) {
        throw std::runtime_error("Error writing UTXO set: " + status.ToString());
    }
}

void UTXOSet::Update(const Block& block) {
    leveldb::WriteBatch batch;

    // the tip height reflects this block as it's added after the block is added
    int32_t blockHeight = blockchain->GetChainHeight();

    for (const Transaction& tx : block.GetTransactions()) {
        if (!tx.IsCoinbase()) {
            for (const TransactionInput& vin : tx.GetVin()) {
                std::vector<uint8_t> txid = vin.GetTxid();
                std::string valueStr;

                leveldb::Status status =
                    db->Get(leveldb::ReadOptions(), ByteArrayToSlice(txid), &valueStr);

                if (status.ok()) {
                    std::vector<uint8_t> valueBytes(valueStr.begin(), valueStr.end());
                    TXOutputs outs = TXOutputs::Deserialize(valueBytes);

                    // erase the spent output by its original index
                    outs.outputs.erase(vin.GetVout());

                    // if no outputs remain, we delete the transaction from UTXO set
                    if (outs.outputs.empty()) {
                        batch.Delete(ByteArrayToSlice(txid));
                    } else {
                        // else we update with the remaining outputs and preserve the metadata
                        std::vector<uint8_t> serialized = outs.Serialize();
                        batch.Put(ByteArrayToSlice(txid), ByteArrayToSlice(serialized));
                    }
                } else if (!status.IsNotFound()) {
                    throw std::runtime_error("Error reading UTXO: " + status.ToString());
                }
            }
        }

        // add the new outputs from this transaction with original indices and metadata
        TXOutputs newOutputs;
        newOutputs.isCoinbase = tx.IsCoinbase();
        newOutputs.blockHeight = blockHeight;

        const auto& vout = tx.GetVout();
        for (size_t i = 0; i < vout.size(); i++) {
            newOutputs.outputs[static_cast<int>(i)] = vout[i];
        }

        std::vector<uint8_t> txHash = tx.GetID();
        std::vector<uint8_t> serialized = newOutputs.Serialize();
        batch.Put(ByteArrayToSlice(txHash), ByteArrayToSlice(serialized));
    }

    leveldb::Status status = db->Write(leveldb::WriteOptions(), &batch);
    if (!status.ok()) {
        throw std::runtime_error("Error updating UTXO set: " + status.ToString());
    }
}