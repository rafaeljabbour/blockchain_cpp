
#include <openssl/sha.h>

#include <iostream>
#include <vector>

#include "blockchain.h"
#include "transaction.h"
#include "utils.h"

Transaction::Transaction(const std::vector<uint8_t>& id, const std::vector<TransactionInput>& vin,
                         const std::vector<TransactionOutput>& vout)
    : id(id), vin(vin), vout(vout) {}

void Transaction::SetID() {
    std::vector<uint8_t> serialized = Serialize();

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(serialized.data(), serialized.size(), hash);

    id.assign(hash, hash + SHA256_DIGEST_LENGTH);
}

bool Transaction::IsCoinbase() const {
    return vin.size() == 1 && vin[0].GetTxid().empty() && vin[0].GetVout() == -1;
}

Transaction Transaction::NewCoinbaseTX(const std::string& to, const std::string& data) {
    std::string coinbaseData = data;
    if (coinbaseData.empty()) {
        coinbaseData = "Reward to '" + to + "'";
    }

    TransactionInput txin({}, -1, coinbaseData);
    TransactionOutput txout(SUBSIDY, to);

    Transaction tx({}, {txin}, {txout});
    tx.SetID();

    return tx;
}

Transaction Transaction::NewUTXOTransaction(const std::string& from, const std::string& to,
                                            int amount, Blockchain* bc) {
    std::vector<TransactionInput> inputs;
    std::vector<TransactionOutput> outputs;

    auto [acc, validOutputs] = bc->FindSpendableOutputs(from, amount);

    if (acc < amount) {
        std::cerr << "ERROR: Not enough funds" << std::endl;
        exit(1);
    }

    for (const auto& [txid, outs] : validOutputs) {
        std::vector<uint8_t> txID = HexStringToByteArray(txid);

        for (int out : outs) {
            TransactionInput input(txID, out, from);
            inputs.push_back(input);
        }
    }

    outputs.push_back(TransactionOutput(amount, to));
    if (acc > amount) {
        outputs.push_back(TransactionOutput(acc - amount, from));
    }

    Transaction tx({}, inputs, outputs);
    tx.SetID();

    return tx;
}

std::vector<uint8_t> Transaction::Serialize() const {
    std::vector<uint8_t> result;

    // number of inputs (4 bytes)
    uint32_t vinCount = vin.size();
    for (int i = 0; i < 4; i++) {
        result.push_back((vinCount >> (8 * i)) & 0xFF);
    }

    // each input
    for (const auto& input : vin) {
        std::vector<uint8_t> inputSerialized = input.Serialize();

        // inputs (variable bytes)
        result.insert(result.end(), inputSerialized.begin(), inputSerialized.end());
    }

    // number of outputs (4 bytes)
    uint32_t voutSize = vout.size();
    for (int i = 0; i < 4; i++) {
        result.push_back((voutSize >> (8 * i)) & 0xFF);
    }

    // each output
    for (const auto& output : vout) {
        std::vector<uint8_t> outputSerialized = output.Serialize();

        // output (variable bytes)
        result.insert(result.end(), outputSerialized.begin(), outputSerialized.end());
    }

    return result;
}

Transaction Transaction::Deserialize(const std::vector<uint8_t>& data) {
    Transaction tx;
    size_t offset = 0;

    // number of inputs (4 bytes)
    uint32_t vinSize = 0;
    for (int i = 0; i < 4; i++) {
        vinSize |= (data[offset + i] << (8 * i));
    }
    offset += 4;

    // each input
    for (uint32_t i = 0; i < vinSize; i++) {
        auto [input, bytesRead] = TransactionInput::Deserialize(data, offset);

        // input (variable bytes)
        tx.vin.push_back(input);
        offset += bytesRead;
    }

    // number of outputs (4 bytes)
    uint32_t voutSize = 0;
    for (int i = 0; i < 4; i++) {
        voutSize |= (data[offset + i] << (8 * i));
    }
    offset += 4;

    // each output
    for (uint32_t i = 0; i < voutSize; i++) {
        auto [output, bytesRead] = TransactionOutput::Deserialize(data, offset);
        tx.vout.push_back(output);

        // output (variable bytes)
        offset += bytesRead;
    }

    tx.SetID();

    return tx;
}