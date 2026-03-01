#ifndef PROOF_OF_WORK_H
#define PROOF_OF_WORK_H

#include <openssl/bn.h>

#include <climits>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "config.h"

class Block;

// RAII type alias for BIGNUM
using BN_ptr = std::unique_ptr<BIGNUM, decltype(&BN_free)>;

// maximum nonce search space
inline constexpr int32_t maxNonce = INT32_MAX;

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
        bool Validate() const;

    private:
        std::vector<uint8_t> PrepareData(int32_t nonce) const;
};

#endif