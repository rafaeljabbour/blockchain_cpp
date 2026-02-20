#include "merkleProof.h"

#include "crypto.h"

static std::vector<uint8_t> combineAndHash(const std::vector<uint8_t>& left,
                                           const std::vector<uint8_t>& right) {
    std::vector<uint8_t> combined;
    combined.reserve(left.size() + right.size());
    combined.insert(combined.end(), left.begin(), left.end());
    combined.insert(combined.end(), right.begin(), right.end());
    return SHA256Hash(combined);
}

bool VerifyMerkleProof(const MerkleProof& proof) {
    if (proof.txHash.empty() || proof.merkleRoot.empty()) {
        return false;
    }

    std::vector<uint8_t> current = proof.txHash;

    for (const MerkleProofStep& step : proof.path) {
        if (step.isLeft) {
            // sibling is the left child so combine as sibling | current
            current = combineAndHash(step.hash, current);
        } else {
            // sibling is the right child so combine as current | sibling
            current = combineAndHash(current, step.hash);
        }
    }

    return current == proof.merkleRoot;
}
