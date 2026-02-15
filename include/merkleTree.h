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

        // tag type for constructing a node with data that is already hashed
        struct PreHashed {};

        // leaf node: hashes the raw data
        explicit MerkleNode(const std::vector<uint8_t>& data);
        // parent node: combines two children
        MerkleNode(std::unique_ptr<MerkleNode> left, std::unique_ptr<MerkleNode> right);
        // pre-hashed node: copies hash directly, used for duplicate intermediate nodes
        MerkleNode(const std::vector<uint8_t>& hash, PreHashed);

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