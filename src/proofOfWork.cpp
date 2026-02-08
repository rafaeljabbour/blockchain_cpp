#include "proofOfWork.h"

#include <openssl/bn.h>

#include <iostream>

#include "utils.h"

ProofOfWork::ProofOfWork(const Block* block) : block(block), target(BN_new(), BN_free) {
    if (!target) {
        throw std::runtime_error("Failed to allocate BIGNUM for PoW target");
    }
    BN_one(target.get());
    BN_lshift(target.get(), target.get(), 256 - targetBits);
}

std::vector<uint8_t> ProofOfWork::PrepareData(int nonce) {
    std::vector<uint8_t> data;

    // previous block hash (32 bytes)
    data.insert(data.end(), block->GetPreviousHash().begin(), block->GetPreviousHash().end());

    // hash of all transactions (32 bytes)
    std::vector<uint8_t> txHashBytes = block->HashTransactions();
    data.insert(data.end(), txHashBytes.begin(), txHashBytes.end());

    // timestamp (8 bytes)
    std::vector<uint8_t> timestampBytes = IntToHexByteArray(block->GetTimestamp());
    data.insert(data.end(), timestampBytes.begin(), timestampBytes.end());

    // target bits (8 bytes)
    std::vector<uint8_t> targetBitsBytes = IntToHexByteArray(targetBits);
    data.insert(data.end(), targetBitsBytes.begin(), targetBitsBytes.end());

    // nonce (8 bytes)
    std::vector<uint8_t> nonceBytes = IntToHexByteArray(nonce);
    data.insert(data.end(), nonceBytes.begin(), nonceBytes.end());

    return data;
}

std::pair<int32_t, std::vector<uint8_t>> ProofOfWork::Run() {
    BN_ptr hashInt(BN_new(), BN_free);
    if (!hashInt) {
        throw std::runtime_error("Failed to allocate BIGNUM for PoW hash");
    }

    std::vector<uint8_t> hash;
    int32_t nonce = 0;

    std::cout << "Mining a new block: " << std::endl;

    while (nonce < maxNonce) {
        std::vector<uint8_t> data = PrepareData(nonce);

        hash = SHA256Hash(data);

        std::cout << "\r" << ByteArrayToHexString(hash) << std::flush;

        BN_bin2bn(hash.data(), hash.size(), hashInt.get());

        if (BN_cmp(hashInt.get(), target.get()) == -1) {
            break;
        } else {
            nonce++;
        }
    }
    std::cout << std::endl << std::endl;

    return {nonce, hash};
}

bool ProofOfWork::Validate() {
    BN_ptr hashInt(BN_new(), BN_free);
    if (!hashInt) {
        throw std::runtime_error("Failed to allocate BIGNUM for PoW validation");
    }

    std::vector<uint8_t> data = PrepareData(block->GetNonce());
    std::vector<uint8_t> hash = SHA256Hash(data);

    BN_bin2bn(hash.data(), hash.size(), hashInt.get());
    return BN_cmp(hashInt.get(), target.get()) == -1;
}