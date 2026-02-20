#ifndef MERKLE_PROOF_H
#define MERKLE_PROOF_H

#include <cstdint>
#include <vector>

struct MerkleProofStep {
        std::vector<uint8_t> hash;  // hash of the sibling at the current level
        bool isLeft;                // tells us which sibling it is(left or right)
};

struct MerkleProof {
        std::vector<uint8_t> txHash;        // the transaction we want to verify
        std::vector<uint8_t> txid;          // the transaction's ID
        uint32_t txIndex;                   // the transaction's index in the block
        std::vector<MerkleProofStep> path;  // the path from the transaction to the root
        std::vector<uint8_t> merkleRoot;    // the exected destination
        std::vector<uint8_t> blockHash;     // the block this proof belongs to
        uint32_t blockHeight;               // the block's height
};

bool VerifyMerkleProof(const MerkleProof& proof);

#endif
