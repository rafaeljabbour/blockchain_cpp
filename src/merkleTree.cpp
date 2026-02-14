#include "merkleTree.h"

#include "transaction.h"
#include "utils.h"

// leaf node constructor
MerkleNode::MerkleNode(const std::vector<uint8_t>& data) : left(nullptr), right(nullptr) {
    this->data = SHA256Hash(data);
}

// parent node constructor
MerkleNode::MerkleNode(std::unique_ptr<MerkleNode> left, std::unique_ptr<MerkleNode> right)
    : left(std::move(left)), right(std::move(right)) {
    // concatenate children then hash
    std::vector<uint8_t> combined;
    combined.insert(combined.end(), this->left->data.begin(), this->left->data.end());
    combined.insert(combined.end(), this->right->data.begin(), this->right->data.end());

    this->data = SHA256Hash(combined);
}

// tree constructor
MerkleTree::MerkleTree(const std::vector<Transaction>& transactions) {
    // serialized transactions
    std::vector<std::vector<uint8_t>> data;
    for (const Transaction& tx : transactions) {
        data.push_back(tx.Serialize());
    }

    // if odd number of transactions, duplicate the last one
    if (data.size() % 2 != 0) {
        data.push_back(data.back());
    }

    // leaf nodes
    std::vector<std::unique_ptr<MerkleNode>> nodes;
    for (const auto& txData : data) {
        nodes.push_back(std::make_unique<MerkleNode>(txData));
    }

    // construct tree bottom-up
    while (nodes.size() > 1) {
        std::vector<std::unique_ptr<MerkleNode>> newLevel;

        for (size_t i = 0; i < nodes.size(); i += 2) {
            auto left = std::move(nodes[i]);
            auto right = std::move(nodes[i + 1]);
            newLevel.push_back(std::make_unique<MerkleNode>(std::move(left), std::move(right)));
        }

        nodes = std::move(newLevel);
    }

    rootNode = std::move(nodes[0]);
}