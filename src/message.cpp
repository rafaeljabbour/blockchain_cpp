#include "message.h"

#include <cstring>
#include <stdexcept>

#include "utils.h"

Message::Message(const std::array<uint8_t, MAGIC_LENGTH>& magic, const std::string& command,
                 const std::vector<uint8_t>& payload)
    : magic(magic), payload(payload) {
    if (command.length() > 12) {
        throw std::runtime_error("Command name cannot exceed 12 characters");
    }

    this->command = CreateCommand(command);
    this->payloadLength = static_cast<uint32_t>(payload.size());
    this->checksum = CalculateChecksum(payload);
}

std::string Message::GetCommandString() const {
    // find the first null byte
    size_t len = 0;
    while (len < 12 && command[len] != '\0') {
        len++;
    }
    return std::string(command.data(), len);
}

std::vector<uint8_t> Message::Serialize() const {
    std::vector<uint8_t> result;

    // magic (4 bytes)
    result.insert(result.end(), magic.begin(), magic.end());

    // command (12 bytes)
    result.insert(result.end(), command.begin(), command.end());

    // length (4 bytes)
    WriteUint32(result, payloadLength);

    // checksum (4 bytes)
    result.insert(result.end(), checksum.begin(), checksum.end());

    // payload (variable bytes)
    result.insert(result.end(), payload.begin(), payload.end());

    return result;
}

Message Message::Deserialize(const std::vector<uint8_t>& data) {
    Message msg;
    size_t offset = 0;

    // minimum message size: 4 (magic) + 12 (command) + 4 (length) + 4 (checksum) = 24 bytes
    if (data.size() < 24) {
        throw std::runtime_error("Message data too small to deserialize");
    }

    // magic (4 bytes)
    std::copy(data.begin() + offset, data.begin() + offset + 4, msg.magic.begin());
    offset += 4;

    if (msg.magic != MAGIC_CUSTOM) {
        throw std::runtime_error("Invalid network magic number");
    }

    // command (12 bytes)
    std::copy(data.begin() + offset, data.begin() + offset + 12, msg.command.begin());
    offset += 12;

    // length (4 bytes)
    msg.payloadLength = ReadUint32(data, offset);
    offset += 4;

    // checksum (4 bytes)
    std::copy(data.begin() + offset, data.begin() + offset + 4, msg.checksum.begin());
    offset += 4;

    // payload (variable bytes)
    if (offset + msg.payloadLength > data.size()) {
        throw std::runtime_error("Message data truncated: payload extends past end");
    }

    msg.payload =
        std::vector<uint8_t>(data.begin() + offset, data.begin() + offset + msg.payloadLength);

    // verify checksum
    std::array<uint8_t, CHECKSUM_LENGTH> calculatedChecksum = CalculateChecksum(msg.payload);
    if (calculatedChecksum != msg.checksum) {
        throw std::runtime_error("Message checksum verification failed");
    }

    return msg;
}

Message Message::DeserializeHeader(const std::vector<uint8_t>& data) {
    Message msg;

    if (data.size() < 24) {
        throw std::runtime_error("Message header data too small");
    }

    size_t offset = 0;

    // magic (4 bytes)
    std::copy(data.begin() + offset, data.begin() + offset + 4, msg.magic.begin());
    offset += 4;

    // validate magic matches the network
    if (msg.magic != MAGIC_CUSTOM) {
        throw std::runtime_error("Invalid network magic number");
    }

    // command (12 bytes)
    std::copy(data.begin() + offset, data.begin() + offset + 12, msg.command.begin());
    offset += 12;

    // payload length (4 bytes)
    msg.payloadLength = ReadUint32(data, offset);
    offset += 4;

    // checksum (4 bytes)
    std::copy(data.begin() + offset, data.begin() + offset + 4, msg.checksum.begin());

    return msg;
}

std::array<uint8_t, CHECKSUM_LENGTH> CalculateChecksum(const std::vector<uint8_t>& payload) {
    // checksum is the first 4 bytes of SHA256(SHA256(payload))
    std::vector<uint8_t> hash = SHA256Hash(SHA256Hash(payload));

    std::array<uint8_t, CHECKSUM_LENGTH> checksum;
    std::copy(hash.begin(), hash.begin() + CHECKSUM_LENGTH, checksum.begin());

    return checksum;
}

std::array<char, COMMAND_LENGTH> CreateCommand(const std::string& cmd) {
    if (cmd.length() > 12) {
        throw std::runtime_error("Command name cannot exceed 12 characters");
    }

    std::array<char, COMMAND_LENGTH> command;
    command.fill('\0');

    // copy command string into the array
    std::copy(cmd.begin(), cmd.end(), command.begin());

    return command;
}