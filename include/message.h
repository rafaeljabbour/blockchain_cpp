#ifndef MESSAGE_H
#define MESSAGE_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

inline constexpr uint32_t MAGIC_LENGTH = 4;
inline constexpr uint32_t COMMAND_LENGTH = 12;
inline constexpr uint32_t CHECKSUM_LENGTH = 4;

// the magic number for the blockchain
inline const std::array<uint8_t, MAGIC_LENGTH> MAGIC_CUSTOM = {0xCA, 0xFE, 0xBA, 0xBE};

// the message commands are max 12 bytes, null-padded
inline const char CMD_VERSION[] = "version";
inline const char CMD_VERACK[] = "verack";
inline const char CMD_GETBLOCKS[] = "getblocks";
inline const char CMD_INV[] = "inv";
inline const char CMD_GETDATA[] = "getdata";
inline const char CMD_BLOCK[] = "block";
inline const char CMD_TX[] = "tx";
inline const char CMD_ADDR[] = "addr";
inline const char CMD_PING[] = "ping";
inline const char CMD_PONG[] = "pong";

class Message {
    private:
        std::array<uint8_t, MAGIC_LENGTH> magic;
        std::array<char, COMMAND_LENGTH> command;
        uint32_t payloadLength;
        std::array<uint8_t, CHECKSUM_LENGTH> checksum;
        std::vector<uint8_t> payload;

    public:
        Message() = default;
        Message(const std::array<uint8_t, MAGIC_LENGTH>& magic, const std::string& command,
                const std::vector<uint8_t>& payload);

        const std::array<uint8_t, MAGIC_LENGTH>& GetMagic() const { return magic; }
        const std::array<char, COMMAND_LENGTH>& GetCommand() const { return command; }
        uint32_t GetPayloadLength() const { return payloadLength; }
        const std::array<uint8_t, CHECKSUM_LENGTH>& GetChecksum() const { return checksum; }
        const std::vector<uint8_t>& GetPayload() const { return payload; }

        // strips null padding
        std::string GetCommandString() const;

        std::vector<uint8_t> Serialize() const;
        static Message Deserialize(const std::vector<uint8_t>& data);
        static Message DeserializeHeader(const std::vector<uint8_t>& data);
};

// network helper functions
std::array<uint8_t, CHECKSUM_LENGTH> CalculateChecksum(const std::vector<uint8_t>& payload);
std::array<char, COMMAND_LENGTH> CreateCommand(const std::string& cmd);

#endif
