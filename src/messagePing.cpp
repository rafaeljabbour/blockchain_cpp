#include "messagePing.h"

#include <random>
#include <stdexcept>

#include "utils.h"

MessagePing::MessagePing(uint64_t nonce) : nonce(nonce) {}

std::vector<uint8_t> MessagePing::Serialize() const {
    std::vector<uint8_t> result;

    // nonce (8 bytes)
    WriteUint64(result, nonce);

    return result;
}

MessagePing MessagePing::Deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 8) {
        throw std::runtime_error("MessagePing data too small to deserialize");
    }

    uint64_t nonce = ReadUint64(data, 0);
    return MessagePing(nonce);
}

std::pair<Message, uint64_t> CreatePingMessage() {
    // random nonce for ping identification
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    uint64_t nonce = dis(gen);

    MessagePing ping(nonce);
    Message msg(MAGIC_CUSTOM, CMD_PING, ping.Serialize());

    return {std::move(msg), nonce};
}

Message CreatePongMessage(uint64_t nonce) {
    MessagePong pong(nonce);
    return Message(MAGIC_CUSTOM, CMD_PONG, pong.Serialize());
}