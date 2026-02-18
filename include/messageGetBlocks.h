#ifndef MESSAGEGETBLOCKS_H
#define MESSAGEGETBLOCKS_H

#include <cstdint>
#include <vector>

class MessageGetBlocks {
    private:
        // best known block hash (32 bytes)
        std::vector<uint8_t> tipHash;

    public:
        explicit MessageGetBlocks(const std::vector<uint8_t>& tipHash);

        const std::vector<uint8_t>& GetTipHash() const { return tipHash; }

        std::vector<uint8_t> Serialize() const;
        static MessageGetBlocks Deserialize(const std::vector<uint8_t>& data);
};

#endif