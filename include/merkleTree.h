#ifndef MERKLETREE_H
#define MERKLETREE_H

#include <cstdint>
#include <vector>

#include "merkleProof.h"

class Transaction;

// the tree is stored as a flat list of levels:
// levels[0] = leaf hashes (SHA256 of each serialized tx)
// levels[1] = parent hashes of pairs of leaves
// levels[N] = [ root hash ]
class MerkleTree {
    private:
        std::vector<std::vector<std::vector<uint8_t>>> levels;

        static std::vector<uint8_t> combineAndHash(const std::vector<uint8_t>& left,
                                                   const std::vector<uint8_t>& right);

    public:
        explicit MerkleTree(const std::vector<Transaction>& transactions);

        ~MerkleTree() = default;

        // prevent copying
        MerkleTree(const MerkleTree&) = delete;
        MerkleTree& operator=(const MerkleTree&) = delete;

        MerkleTree(MerkleTree&&) = default;
        MerkleTree& operator=(MerkleTree&&) = default;

        const std::vector<uint8_t>& GetRootHash() const { return levels.back()[0]; }

        MerkleProof GenerateProof(uint32_t txIndex) const;
        static bool VerifyProof(const MerkleProof& proof);
};

#endif