#include "netAddr.h"

#include <ctime>
#include <sstream>
#include <stdexcept>

#include "serialization.h"

NetAddr::NetAddr(uint64_t services, const std::string& ipv4, uint16_t port)
    : time(0), services(services), port(port) {
    // parse IPv4 address string
    std::stringstream ss(ipv4);
    std::string segment;
    std::vector<uint8_t> ipv4Bytes;

    while (std::getline(ss, segment, '.')) {
        ipv4Bytes.push_back(static_cast<uint8_t>(std::stoi(segment)));
    }

    if (ipv4Bytes.size() != 4) {
        throw std::runtime_error("Invalid IPv4 address format");
    }

    // map IPv4 to IPv6 format
    // format: [10 zero bytes][0xFF][0xFF][4 IPv4 bytes]
    ip.fill(0x00);
    ip[10] = 0xFF;
    ip[11] = 0xFF;
    std::copy(ipv4Bytes.begin(), ipv4Bytes.end(), ip.begin() + 12);
}

std::vector<uint8_t> NetAddr::Serialize(bool includeTime) const {
    std::vector<uint8_t> result;

    // only serialize time if includeTime is true
    if (includeTime) {
        // time (4 bytes)
        WriteUint32(result, time);
    }

    // services (8 bytes)
    WriteUint64(result, services);

    // IP address (16 bytes)
    result.insert(result.end(), ip.begin(), ip.end());

    // port (2 bytes, big-endian for network byte order)
    result.push_back((port >> 8) & 0xFF);
    result.push_back(port & 0xFF);

    return result;
}

std::pair<NetAddr, size_t> NetAddr::Deserialize(const std::vector<uint8_t>& data, size_t offset,
                                                bool includeTime) {
    NetAddr addr;
    size_t startOffset = offset;

    // time (4 bytes)
    if (includeTime) {
        if (offset + 4 > data.size()) {
            throw std::runtime_error("NetAddr data truncated at time");
        }
        addr.time = ReadUint32(data, offset);
        offset += 4;
    } else {
        addr.time = 0;
    }

    // services (8 bytes)
    if (offset + 8 > data.size()) {
        throw std::runtime_error("NetAddr data truncated at services");
    }
    addr.services = ReadUint64(data, offset);
    offset += 8;

    // IP address (16 bytes)
    if (offset + 16 > data.size()) {
        throw std::runtime_error("NetAddr data truncated at IP");
    }
    std::copy(data.begin() + offset, data.begin() + offset + 16, addr.ip.begin());
    offset += 16;

    // port (2 bytes, big-endian)
    if (offset + 2 > data.size()) {
        throw std::runtime_error("NetAddr data truncated at port");
    }
    addr.port =
        (static_cast<uint16_t>(data[offset]) << 8) | static_cast<uint16_t>(data[offset + 1]);
    offset += 2;

    size_t bytesRead = offset - startOffset;
    return {addr, bytesRead};
}
