#include "proofOfWork.h"

#include <openssl/bn.h>
#include <openssl/sha.h>

#include <iostream>

#include "utils.h"

ProofOfWork::ProofOfWork(const Block* block) {
    target = BN_new();
    BN_one(target);
    BN_lshift(target, target, 256 - targetBits);
    this->block = block;
}

ProofOfWork::~ProofOfWork() { BN_free(target); }

std::vector<uint8_t> ProofOfWork::PrepareData(int nonce) {
    std::vector<uint8_t> data;

    data.insert(data.end(), block->GetPreviousHash().begin(),
                block->GetPreviousHash().end());
    data.insert(data.end(), block->GetData().begin(), block->GetData().end());

    std::string timestampString = IntToHexString(block->GetTimestamp());
    std::string targetBitsString = IntToHexString(targetBits);
    std::string nonceString = IntToHexString(nonce);

    data.insert(data.end(), timestampString.begin(), timestampString.end());
    data.insert(data.end(), targetBitsString.begin(), targetBitsString.end());
    data.insert(data.end(), nonceString.begin(), nonceString.end());

    return data;
}

std::pair<int32_t, std::vector<uint8_t>> ProofOfWork::Run() {
    BIGNUM* hashInt = BN_new();
    std::vector<uint8_t> hash;
    int32_t nonce = 0;

    std::cout << "Mining the block: " << ByteArrayToString(block->GetData())
              << std::endl;

    while (nonce < maxNonce) {
        std::vector<uint8_t> data = PrepareData(nonce);

        uint8_t hashArray[SHA256_DIGEST_LENGTH];
        SHA256(data.data(), data.size(), hashArray);
        hash.assign(hashArray, hashArray + SHA256_DIGEST_LENGTH);

        std::cout << "\r" << ByteArrayToHexString(hash) << std::flush;

        BN_bin2bn(hash.data(), hash.size(), hashInt);

        if (BN_cmp(hashInt, target) == -1) {
            break;
        } else {
            nonce++;
        }
    }
    std::cout << std::endl << std::endl;
    BN_free(hashInt);

    return {nonce, hash};
}

bool ProofOfWork::Validate() {
    BIGNUM* hashInt = BN_new();
    std::vector<uint8_t> data = PrepareData(block->GetNonce());

    uint8_t hashArray[SHA256_DIGEST_LENGTH];
    SHA256(data.data(), data.size(), hashArray);

    BN_bin2bn(hashArray, SHA256_DIGEST_LENGTH, hashInt);
    bool isValid = (BN_cmp(hashInt, target) == -1);

    BN_free(hashInt);

    return isValid;
}