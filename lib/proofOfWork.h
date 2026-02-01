#ifndef PROOF_OF_WORK_H
#define PROOF_OF_WORK_H

#include "block.h"
#include <openssl/bn.h>
#include <vector>
#include <cstdint>
#include <utility>

const int32_t targetBits = 17;
const int32_t maxNonce  = INT32_MAX;

class ProofOfWork {
    private:
        const Block* block;
        BIGNUM* target; // upperbound for valid hash value

    public:
        ProofOfWork(const Block* block);
        ~ProofOfWork();
        std::pair<int32_t, std::vector<uint8_t>> Run();
        bool Validate();

    private:
        std::vector<uint8_t> PrepareData(int nonce);
};

#endif