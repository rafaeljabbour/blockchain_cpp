#ifndef PROOF_OF_WORK_H
#define PROOF_OF_WORK_H

#include <openssl/bn.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "block.h"

const int32_t targetBits = 17;
const int32_t maxNonce = INT32_MAX;

// RAII type alias for BIGNUM
using BN_ptr = std::unique_ptr<BIGNUM, decltype(&BN_free)>;

class ProofOfWork {
    private:
        const Block* block;
        BN_ptr target;  // upperbound for valid hash value

    public:
        ProofOfWork(const Block* block);
        ~ProofOfWork() = default;

        // prevent copying
        ProofOfWork(const ProofOfWork&) = delete;
        ProofOfWork& operator=(const ProofOfWork&) = delete;

        std::pair<int32_t, std::vector<uint8_t>> Run();
        bool Validate();

    private:
        std::vector<uint8_t> PrepareData(int nonce);
};

#endif