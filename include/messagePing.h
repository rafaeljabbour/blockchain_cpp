#ifndef MESSAGEPING_H
#define MESSAGEPING_H

#include <cstdint>
#include <utility>
#include <vector>

#include "message.h"

// we send ping to check if a peer is still alive
class MessagePing {
    private:
        uint64_t nonce;

    public:
        explicit MessagePing(uint64_t nonce);

        uint64_t GetNonce() const { return nonce; }

        std::vector<uint8_t> Serialize() const;
        static MessagePing Deserialize(const std::vector<uint8_t>& data);
};

// we reply pong to inform we are alive
using MessagePong = MessagePing;

std::pair<Message, uint64_t> CreatePingMessage();

Message CreatePongMessage(uint64_t nonce);

#endif