#include "merkleTree.h"

#include <stdexcept>
#include <string>

#include "crypto.h"
#include "transaction.h"

std::vector<uint8_t> MerkleTree::combineAndHash(const std::vector<uint8_t>& left,
                                                const std::vector<uint8_t>& right) {
    std::vector<uint8_t> combined;
    combined.reserve(left.size() + right.size());
    combined.insert(combined.end(), left.begin(), left.end());
    combined.insert(combined.end(), right.begin(), right.end());
    return SHA256Hash(combined);
}

MerkleTree::MerkleTree(const std::vector<Transaction>& transactions) {
    if (transactions.empty()) {
        throw std::invalid_argument("Cannot build Merkle tree from empty transaction list");
    }

    // at level 0, one leaf hash per transaction
    std::vector<std::vector<uint8_t>> currentLevel;
    currentLevel.reserve(transactions.size());
    for (const Transaction& tx : transactions) {
        currentLevel.push_back(SHA256Hash(tx.Serialize()));
    }

    // pad odd length levels by duplicating the last hash
    if (currentLevel.size() % 2 != 0) {
        currentLevel.push_back(currentLevel.back());
    }

    levels.push_back(currentLevel);

    // reduce level by level until we reach the single root
    while (currentLevel.size() > 1) {
        if (currentLevel.size() % 2 != 0) {
            currentLevel.push_back(currentLevel.back());
        }

        std::vector<std::vector<uint8_t>> nextLevel;
        nextLevel.reserve(currentLevel.size() / 2);
        for (size_t i = 0; i + 1 < currentLevel.size(); i += 2) {
            nextLevel.push_back(combineAndHash(currentLevel[i], currentLevel[i + 1]));
        }

        levels.push_back(nextLevel);
        currentLevel = nextLevel;
    }
}

MerkleProof MerkleTree::GenerateProof(uint32_t txIndex) const {
    if (levels.empty() || txIndex >= levels[0].size()) {
        throw std::out_of_range("txIndex " + std::to_string(txIndex) +
                                " out of range (leaf level has " +
                                std::to_string(levels[0].size()) + " entries)");
    }

    MerkleProof proof;
    proof.txHash = levels[0][txIndex];
    proof.txIndex = txIndex;
    proof.merkleRoot = levels.back()[0];

    uint32_t idx = txIndex;

    // walk from the leaf level up, collecting the sibling at each level
    for (size_t level = 0; level + 1 < levels.size(); level++) {
        const auto& currentLevel = levels[level];

        // both lines below get the sibling index
        // uint32_t siblingIdx = (idx % 2 == 0) ? idx + 1 : idx - 1;
        uint32_t siblingIdx = idx ^ 1;

        // for odd length levels, the sibling is the duplicate
        if (siblingIdx >= currentLevel.size()) {
            siblingIdx = idx;
        }

        MerkleProofStep step;
        step.hash = currentLevel[siblingIdx];
        step.isLeft = (idx % 2 == 1);
        proof.path.push_back(step);

        idx /= 2;
    }

    return proof;
}

bool MerkleTree::VerifyProof(const MerkleProof& proof) { return VerifyMerkleProof(proof); }