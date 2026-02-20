#include "utxoSet.h"

#include <leveldb/iterator.h>
#include <leveldb/write_batch.h>

#include <memory>
#include <stdexcept>

#include "block.h"
#include "blockchain.h"
#include "utils.h"

UTXOSet::UTXOSet(Blockchain* bc) : blockchain(bc) {
    if (!bc) {
        throw std::invalid_argument("Blockchain pointer cannot be null");
    }
}

std::pair<int, std::map<std::string, std::vector<int>>> UTXOSet::FindSpendableOutputs(
    const std::vector<uint8_t>& pubKeyHash, int amount) const {
    std::map<std::string, std::vector<int>> unspentOutputs;
    int accumulated = 0;
    bool found = false;

    std::unique_ptr<leveldb::Iterator> it(blockchain->db->NewIterator(leveldb::ReadOptions()));

    for (it->SeekToFirst(); it->Valid() && !found; it->Next()) {
        std::string key = it->key().ToString();
        std::string value = it->value().ToString();

        // only read UTXO entries
        if (key.empty() || key[0] != 'u') {
            continue;
        }

        std::string txID = ByteArrayToHexString(std::vector<uint8_t>(key.begin() + 1, key.end()));

        std::vector<uint8_t> valueBytes(value.begin(), value.end());
        TXOutputs outs = TXOutputs::Deserialize(valueBytes);

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

    std::unique_ptr<leveldb::Iterator> it(blockchain->db->NewIterator(leveldb::ReadOptions()));

    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        std::string key = it->key().ToString();
        std::string value = it->value().ToString();

        // skip non-UTXO entries
        if (key.empty() || key[0] != 'u') {
            continue;
        }

        std::vector<uint8_t> valueBytes(value.begin(), value.end());
        TXOutputs outs = TXOutputs::Deserialize(valueBytes);

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

    std::unique_ptr<leveldb::Iterator> it(blockchain->db->NewIterator(leveldb::ReadOptions()));

    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        std::string key = it->key().ToString();

        // Only count UTXO entries
        if (!key.empty() && key[0] == 'u') {
            counter++;
        }
    }

    if (!it->status().ok()) {
        throw std::runtime_error("Error iterating UTXO set: " + it->status().ToString());
    }

    return counter;
}

void UTXOSet::Reindex() {
    leveldb::DB* db = blockchain->db.get();

    // find all the UTXO entries currently there
    std::vector<std::string> keysToDelete;

    std::unique_ptr<leveldb::Iterator> it(db->NewIterator(leveldb::ReadOptions()));
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        std::string key = it->key().ToString();
        if (!key.empty() && key[0] == 'u') {
            // UTXO entry
            keysToDelete.push_back(key);
        }
    }

    if (!it->status().ok()) {
        throw std::runtime_error("Error scanning for UTXO entries: " + it->status().ToString());
    }

    // delete the old UTXO entries
    leveldb::WriteBatch batch;
    for (const auto& key : keysToDelete) {
        batch.Delete(key);
    }

    leveldb::Status status = db->Write(leveldb::WriteOptions(), &batch);
    if (!status.ok()) {
        throw std::runtime_error("Error clearing UTXO set: " + status.ToString());
    }

    // build new UTXO set from the blockchain
    std::map<std::string, TXOutputs> UTXO = blockchain->FindUTXO();

    // write the new UTXO set to the database
    leveldb::WriteBatch newBatch;

    for (const auto& [txID, outs] : UTXO) {
        std::vector<uint8_t> key;
        key.push_back('u');
        std::vector<uint8_t> txHash = HexStringToByteArray(txID);
        key.insert(key.end(), txHash.begin(), txHash.end());

        std::vector<uint8_t> value = outs.Serialize();

        newBatch.Put(ByteArrayToSlice(key), ByteArrayToSlice(value));
    }

    status = db->Write(leveldb::WriteOptions(), &newBatch);
    if (!status.ok()) {
        throw std::runtime_error("Error writing UTXO set: " + status.ToString());
    }
}

void UTXOSet::Update(const Block& block) {
    leveldb::DB* db = blockchain->db.get();
    leveldb::WriteBatch batch;

    for (const Transaction& tx : block.GetTransactions()) {
        if (!tx.IsCoinbase()) {
            for (const TransactionInput& vin : tx.GetVin()) {
                std::vector<uint8_t> txid = vin.GetTxid();
                std::string valueStr;

                std::vector<uint8_t> key;
                key.push_back('u');
                key.insert(key.end(), txid.begin(), txid.end());

                leveldb::Status status =
                    db->Get(leveldb::ReadOptions(), ByteArrayToSlice(key), &valueStr);

                if (status.ok()) {
                    std::vector<uint8_t> valueBytes(valueStr.begin(), valueStr.end());
                    TXOutputs outs = TXOutputs::Deserialize(valueBytes);

                    // erase the spent output by its original index
                    outs.outputs.erase(vin.GetVout());

                    // if no outputs remain, we delete the transaction from UTXO set
                    if (outs.outputs.empty()) {
                        batch.Delete(ByteArrayToSlice(key));
                    } else {
                        // else we update with the remaining outputs
                        std::vector<uint8_t> serialized = outs.Serialize();
                        batch.Put(ByteArrayToSlice(key), ByteArrayToSlice(serialized));
                    }
                } else if (!status.IsNotFound()) {
                    throw std::runtime_error("Error reading UTXO: " + status.ToString());
                }
            }
        }

        // add the new outputs from this transaction with original indices
        TXOutputs newOutputs;
        const auto& vout = tx.GetVout();
        for (size_t i = 0; i < vout.size(); i++) {
            newOutputs.outputs[static_cast<int>(i)] = vout[i];
        }

        std::vector<uint8_t> key;
        key.push_back('u');
        std::vector<uint8_t> txHash = tx.GetID();
        key.insert(key.end(), txHash.begin(), txHash.end());

        std::vector<uint8_t> serialized = newOutputs.Serialize();
        batch.Put(ByteArrayToSlice(key), ByteArrayToSlice(serialized));
    }

    // atomic write to update db
    leveldb::Status status = db->Write(leveldb::WriteOptions(), &batch);
    if (!status.ok()) {
        throw std::runtime_error("Error updating UTXO set: " + status.ToString());
    }
}