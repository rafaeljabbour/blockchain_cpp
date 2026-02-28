#ifndef PROOF_OF_WORK_H
#define PROOF_OF_WORK_H

#include <openssl/bn.h>

#include <climits>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

class Block;

// RAII type alias for BIGNUM
using BN_ptr = std::unique_ptr<BIGNUM, decltype(&BN_free)>;

// the consensus parameters are as follows:
// hash must have at least 17 leading zeros
inline constexpr int32_t INITIAL_BITS = 17;
// retarget every 2016 blocks
inline constexpr int32_t RETARGET_INTERVAL = 2016;
// expected time for one retarget period (2016 blocks × 10 min = 2 weeks)
inline constexpr int64_t TARGET_TIMESPAN = 2016LL * 10 * 60;  // 1 209 600 seconds
// hard difficulty bounds to prevent runaway adjustments
inline constexpr int32_t MIN_BITS = 1;    // easiest target
inline constexpr int32_t MAX_BITS = 255;  // hardest target
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