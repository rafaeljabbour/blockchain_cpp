#include "transaction.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>

#include <iostream>

#include "blockchain.h"
#include "utils.h"
#include "wallet.h"
#include "wallets.h"

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
            std::cerr << "ERROR: Previous transaction is not correct" << std::endl;
            exit(1);
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
        EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
        if (!mdctx) {
            std::cerr << "Error: Failed to create EVP_MD_CTX" << std::endl;
            exit(1);
        }

        size_t sigLen = 0;
        unsigned char* sig = nullptr;

        // initialize and setup signing with SHA256 and priv key, and calculate space for signature
        if (EVP_DigestSignInit(mdctx, nullptr, EVP_sha256(), nullptr, privKey) <= 0 ||
            EVP_DigestSign(mdctx, nullptr, &sigLen, txCopy.id.data(), txCopy.id.size()) <= 0) {
            std::cerr << "Error: Failed to initialize signing" << std::endl;
            EVP_MD_CTX_free(mdctx);
            exit(1);
        }

        // we allocate memory to hold the signature
        sig = (unsigned char*)OPENSSL_malloc(sigLen);
        if (!sig) {
            std::cerr << "Error: Failed to allocate signature buffer" << std::endl;
            EVP_MD_CTX_free(mdctx);
            exit(1);
        }

        // signing
        if (EVP_DigestSign(mdctx, sig, &sigLen, txCopy.id.data(), txCopy.id.size()) <= 0) {
            std::cerr << "Error: Failed to sign transaction" << std::endl;
            OPENSSL_free(sig);
            EVP_MD_CTX_free(mdctx);
            exit(1);
        }

        std::vector<uint8_t> signature(sig, sig + sigLen);
        OPENSSL_free(sig);
        EVP_MD_CTX_free(mdctx);

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
            std::cerr << "ERROR: Previous transaction is not correct" << std::endl;
            exit(1);
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
        EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr);
        if (!pctx) {
            std::cerr << "Error: Failed to create context for public key" << std::endl;
            return false;
        }


        if (EVP_PKEY_fromdata_init(pctx) <= 0) {
            std::cerr << "Error: Failed to init fromdata" << std::endl;
            EVP_PKEY_CTX_free(pctx);
            return false;
        }

        OSSL_PARAM params[3];
        params[0] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
                                                     const_cast<char*>("secp256k1"), 0);
        params[1] = OSSL_PARAM_construct_octet_string(
            OSSL_PKEY_PARAM_PUB_KEY, const_cast<uint8_t*>(pubKeyBytes.data()), pubKeyBytes.size());
        params[2] = OSSL_PARAM_construct_end();

        // calculate EVP_KEY for secp256k1 using the parameters
        EVP_PKEY* pubKey = nullptr;
        if (EVP_PKEY_fromdata(pctx, &pubKey, EVP_PKEY_PUBLIC_KEY, params) <= 0) {
            std::cerr << "Error: Failed to create public key from bytes" << std::endl;
            EVP_PKEY_CTX_free(pctx);
            return false;
        }

        EVP_PKEY_CTX_free(pctx);

        // create context for verifying signature
        EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
        if (!mdctx) {
            std::cerr << "Error: Failed to create EVP_MD_CTX" << std::endl;
            EVP_PKEY_free(pubKey);
            return false;
        }

        const std::vector<uint8_t>& signature = vin[inID].GetSignature();

        // initialize the verification, setup verification with SHA256 and pub key
        if (EVP_DigestVerifyInit(mdctx, nullptr, EVP_sha256(), nullptr, pubKey) <= 0) {
            std::cerr << "Error: Failed to initialize verification" << std::endl;
            EVP_MD_CTX_free(mdctx);
            EVP_PKEY_free(pubKey);
            return false;
        }

        // compare against actual signature
        int result = EVP_DigestVerify(mdctx, signature.data(), signature.size(), txCopy.id.data(),
                                      txCopy.id.size());

        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pubKey);

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
        coinbaseData = "Reward to '" + to + "'";
    }

    TransactionInput txin({}, -1, {}, StringToBytes(coinbaseData));
    TransactionOutput txout = NewTXOutput(SUBSIDY, to);

    Transaction tx({}, {txin}, {txout});
    tx.id = tx.Hash();

    return tx;
}

Transaction Transaction::NewUTXOTransaction(const std::string& from, const std::string& to,
                                            int amount, Blockchain* bc) {
    std::vector<TransactionInput> inputs;
    std::vector<TransactionOutput> outputs;

    // load wallets and get sender's wallet
    Wallets wallets;
    Wallet* wallet = wallets.GetWallet(from);
    if (!wallet) {
        std::cerr << "ERROR: Wallet not found for address: " << from << std::endl;
        exit(1);
    }

    auto [acc, validOutputs] =
        bc->FindSpendableOutputs(Wallet::HashPubKey(wallet->GetPublicKey()), amount);

    if (acc < amount) {
        std::cerr << "ERROR: Not enough funds" << std::endl;
        exit(1);
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
    bc->SignTransaction(&tx, wallet);

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

    tx.id = tx.Hash();

    return tx;
}