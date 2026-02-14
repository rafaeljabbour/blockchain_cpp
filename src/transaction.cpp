#include "transaction.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>

#include <memory>
#include <random>

#include "blockchain.h"
#include "utils.h"
#include "utxoSet.h"
#include "wallet.h"
#include "wallets.h"

// RAII type aliases for resources
using EVP_MD_CTX_ptr = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
using EVP_PKEY_ptr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using EVP_PKEY_CTX_ptr = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;

Transaction::Transaction(const std::vector<uint8_t>& id, const std::vector<TransactionInput>& vin,
                         const std::vector<TransactionOutput>& vout)
    : id(id), vin(vin), vout(vout) {}

bool Transaction::IsCoinbase() const {
    return vin.size() == 1 && vin[0].GetTxid().empty() && vin[0].GetVout() == -1;
}

std::vector<uint8_t> Transaction::Hash() const { return SHA256Hash(Serialize()); }

void Transaction::Sign(EVP_PKEY* privKey, const std::map<std::string, Transaction>& prevTXs) {
    if (IsCoinbase()) {
        return;
    }

    // Verify all previous transactions exist
    for (const auto& vin : vin) {
        std::string txID = ByteArrayToHexString(vin.GetTxid());
        if (prevTXs.find(txID) == prevTXs.end() || prevTXs.at(txID).GetID().empty()) {
            throw std::runtime_error("Previous transaction is not correct");
        }
    }

    Transaction txCopy = TrimmedCopy();

    // signing each input
    for (size_t inID = 0; inID < txCopy.vin.size(); inID++) {
        std::string txID = ByteArrayToHexString(txCopy.vin[inID].GetTxid());
        const Transaction& prevTx = prevTXs.at(txID);

        txCopy.vin[inID].signature = {};
        txCopy.vin[inID].pubKey = prevTx.vout[txCopy.vin[inID].GetVout()].GetPubKeyHash();
        txCopy.id = txCopy.Hash();
        txCopy.vin[inID].pubKey = {};

        // set context for signing
        EVP_MD_CTX_ptr mdctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
        if (!mdctx) {
            throw std::runtime_error("Failed to create EVP_MD_CTX for signing");
        }

        size_t sigLen = 0;

        // initialize and setup signing with SHA256 and priv key, and calculate space for signature
        if (EVP_DigestSignInit(mdctx.get(), nullptr, EVP_sha256(), nullptr, privKey) <= 0 ||
            EVP_DigestSign(mdctx.get(), nullptr, &sigLen, txCopy.id.data(), txCopy.id.size()) <=
                0) {
            throw std::runtime_error("Failed to initialize signing");
        }

        // allocate buffer to hold signature
        std::vector<unsigned char> sigBuf(sigLen);

        // signing
        if (EVP_DigestSign(mdctx.get(), sigBuf.data(), &sigLen, txCopy.id.data(),
                           txCopy.id.size()) <= 0) {
            throw std::runtime_error("Failed to sign transaction");
        }

        std::vector<uint8_t> signature(sigBuf.data(), sigBuf.data() + sigLen);

        // Store signature in the actual transaction and not the copy
        this->vin[inID].signature = signature;
    }
}

bool Transaction::Verify(const std::map<std::string, Transaction>& prevTXs) const {
    if (IsCoinbase()) {
        return true;
    }

    // verify all previous transactions exist
    for (const auto& vin : vin) {
        std::string txID = ByteArrayToHexString(vin.GetTxid());
        if (prevTXs.find(txID) == prevTXs.end() || prevTXs.at(txID).GetID().empty()) {
            throw std::runtime_error("Previous transaction is not correct");
        }
    }

    Transaction txCopy = TrimmedCopy();

    // verify each tx
    for (size_t inID = 0; inID < vin.size(); inID++) {
        std::string txID = ByteArrayToHexString(txCopy.vin[inID].GetTxid());
        const Transaction& prevTx = prevTXs.at(txID);

        txCopy.vin[inID].signature = {};
        // sign it, save it, then revert
        txCopy.vin[inID].pubKey = prevTx.vout[txCopy.vin[inID].GetVout()].GetPubKeyHash();
        txCopy.id = txCopy.Hash();
        txCopy.vin[inID].pubKey = {};

        // extract public key from input
        const std::vector<uint8_t>& pubKeyBytes = vin[inID].GetPubKey();

        // create EVP_PKEY from public key bytes using fromdata
        EVP_PKEY_CTX_ptr pctx(EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr),
                              EVP_PKEY_CTX_free);
        if (!pctx) {
            throw std::runtime_error("Failed to create context for public key");
        }

        if (EVP_PKEY_fromdata_init(pctx.get()) <= 0) {
            throw std::runtime_error("Failed to init fromdata");
        }

        OSSL_PARAM params[3];
        params[0] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
                                                     const_cast<char*>("secp256k1"), 0);
        params[1] = OSSL_PARAM_construct_octet_string(
            OSSL_PKEY_PARAM_PUB_KEY, const_cast<uint8_t*>(pubKeyBytes.data()), pubKeyBytes.size());
        params[2] = OSSL_PARAM_construct_end();

        // calculate EVP_KEY for secp256k1 using the parameters
        EVP_PKEY* rawPubKey = nullptr;
        if (EVP_PKEY_fromdata(pctx.get(), &rawPubKey, EVP_PKEY_PUBLIC_KEY, params) <= 0) {
            throw std::runtime_error("Failed to create public key from bytes");
        }
        EVP_PKEY_ptr pubKey(rawPubKey, EVP_PKEY_free);

        // create context for verifying signature
        EVP_MD_CTX_ptr mdctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
        if (!mdctx) {
            throw std::runtime_error("Failed to create EVP_MD_CTX for verification");
        }

        const std::vector<uint8_t>& signature = vin[inID].GetSignature();

        // initialize the verification, setup verification with SHA256 and pub key
        if (EVP_DigestVerifyInit(mdctx.get(), nullptr, EVP_sha256(), nullptr, pubKey.get()) <= 0) {
            throw std::runtime_error("Failed to initialize verification");
        }

        // compare against actual signature
        int result = EVP_DigestVerify(mdctx.get(), signature.data(), signature.size(),
                                      txCopy.id.data(), txCopy.id.size());

        if (result != 1) {
            return false;
        }
    }

    return true;
}

Transaction Transaction::TrimmedCopy() const {
    std::vector<TransactionInput> inputs;
    std::vector<TransactionOutput> outputs;

    for (const auto& vin : this->vin) {
        // trimmed to get rid of the signature
        inputs.push_back(TransactionInput(vin.GetTxid(), vin.GetVout(), {}, {}));
    }

    for (const auto& vout : this->vout) {
        outputs.push_back(TransactionOutput(vout.GetValue(), vout.GetPubKeyHash()));
    }

    return Transaction(id, inputs, outputs);
}

Transaction Transaction::NewCoinbaseTX(const std::string& to, const std::string& data) {
    std::string coinbaseData = data;
    if (coinbaseData.empty()) {
        // random data for uniqueness and privacy
        std::vector<uint8_t> randData(20);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);

        for (auto& byte : randData) {
            byte = static_cast<uint8_t>(dis(gen));
        }

        coinbaseData = ByteArrayToHexString(randData);
    }

    TransactionInput txin({}, -1, {}, StringToBytes(coinbaseData));
    TransactionOutput txout = NewTXOutput(SUBSIDY, to);

    Transaction tx({}, {txin}, {txout});
    tx.id = tx.Hash();

    return tx;
}

Transaction Transaction::NewUTXOTransaction(const std::string& from, const std::string& to,
                                            int amount, UTXOSet* utxoSet) {
    std::vector<TransactionInput> inputs;
    std::vector<TransactionOutput> outputs;

    // load wallets and get sender's wallet
    Wallets wallets;
    Wallet* wallet = wallets.GetWallet(from);
    if (!wallet) {
        throw std::runtime_error("Wallet not found for address: " + from);
    }

    auto [acc, validOutputs] =
        utxoSet->FindSpendableOutputs(Wallet::HashPubKey(wallet->GetPublicKey()), amount);

    if (acc < amount) {
        throw std::runtime_error("Not enough funds");
    }

    // build a list of inputs
    for (const auto& [txid, outs] : validOutputs) {
        std::vector<uint8_t> txID = HexStringToByteArray(txid);

        for (int out : outs) {
            TransactionInput input(txID, out, {}, wallet->GetPublicKey());
            inputs.push_back(input);
        }
    }

    // build a list of outputs
    outputs.push_back(NewTXOutput(amount, to));
    if (acc > amount) {
        outputs.push_back(NewTXOutput(acc - amount, from));  // change
    }

    Transaction tx({}, inputs, outputs);
    tx.id = tx.Hash();
    utxoSet->blockchain->SignTransaction(&tx, wallet);

    return tx;
}

std::vector<uint8_t> Transaction::Serialize() const {
    std::vector<uint8_t> result;

    // number of inputs (4 bytes)
    WriteUint32(result, static_cast<uint32_t>(vin.size()));

    // each input
    for (const auto& input : vin) {
        std::vector<uint8_t> inputSerialized = input.Serialize();

        // inputs (variable bytes)
        result.insert(result.end(), inputSerialized.begin(), inputSerialized.end());
    }

    // number of outputs (4 bytes)
    WriteUint32(result, static_cast<uint32_t>(vout.size()));

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

    if (data.size() < 8) {
        throw std::runtime_error("Transaction data too small to deserialize");
    }

    // number of inputs (4 bytes)
    uint32_t vinSize = ReadUint32(data, offset);
    offset += 4;

    // each input
    for (uint32_t i = 0; i < vinSize; i++) {
        auto [input, bytesRead] = TransactionInput::Deserialize(data, offset);

        // input (variable bytes)
        tx.vin.push_back(input);
        offset += bytesRead;
    }

    // number of outputs (4 bytes)
    uint32_t voutSize = ReadUint32(data, offset);
    offset += 4;

    // each output
    for (uint32_t i = 0; i < voutSize; i++) {
        auto [output, bytesRead] = TransactionOutput::Deserialize(data, offset);

        // output (variable bytes)
        tx.vout.push_back(output);
        offset += bytesRead;
    }

    tx.id = tx.Hash();

    return tx;
}