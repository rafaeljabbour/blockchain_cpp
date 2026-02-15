#ifndef NETADDR_H
#define NETADDR_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct NetAddr {
        uint32_t time;               // timestamp for peers, or 0 for version messages
        uint64_t services;           // service flags
        std::array<uint8_t, 16> ip;  // IPv6 address (IPv4 mapped to IPv6, by RFC4291 standard)
        uint16_t port;               // port number

        NetAddr() : time(0), services(0), port(0) { ip.fill(0); }
        NetAddr(uint64_t services, const std::string& ipv4, uint16_t port);

        std::vector<uint8_t> Serialize(bool includeTime = false) const;
        static std::pair<NetAddr, size_t> Deserialize(const std::vector<uint8_t>& data,
                                                      size_t offset, bool includeTime = false);
};

#endif