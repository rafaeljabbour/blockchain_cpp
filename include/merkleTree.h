#ifndef MERKLETREE_H
#define MERKLETREE_H

#include <cstdint>
#include <memory>
#include <vector>

class Transaction;

// node in the Merkle tree
class MerkleNode {
    public:
        std::unique_ptr<MerkleNode> left;
        std::unique_ptr<MerkleNode> right;
        std::vector<uint8_t> data;  // Hash of the data

        MerkleNode(const std::vector<uint8_t>& data);
        MerkleNode(std::unique_ptr<MerkleNode> left, std::unique_ptr<MerkleNode> right);

        ~MerkleNode() = default;
};

// Merkle tree
class MerkleTree {
    private:
        std::unique_ptr<MerkleNode> rootNode;

    public:
        explicit MerkleTree(const std::vector<Transaction>& transactions);

        ~MerkleTree() = default;

        const std::vector<uint8_t>& GetRootHash() const { return rootNode->data; }
};

#endif