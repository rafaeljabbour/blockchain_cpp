#include "addrManager.h"

#include <algorithm>
#include <ctime>
#include <numeric>
#include <random>

std::string AddrManager::MakeKey(const NetAddr& addr) {
    return ExtractIPv4(addr) + ":" + std::to_string(addr.port);
}

std::string AddrManager::ExtractIPv4(const NetAddr& addr) {
    // NetAddr stores IPv4 as IPv4-mapped IPv6: [10 zeros][0xFF][0xFF][4 IPv4 bytes]
    return std::to_string(addr.ip[12]) + "." + std::to_string(addr.ip[13]) + "." +
           std::to_string(addr.ip[14]) + "." + std::to_string(addr.ip[15]);
}

void AddrManager::SetSelfAddr(const std::string& ip, uint16_t port) {
    std::lock_guard<std::mutex> lock(mtx);
    selfAddr = ip + ":" + std::to_string(port);
}

void AddrManager::Add(const NetAddr& addr) {
    std::lock_guard<std::mutex> lock(mtx);

    std::string key = MakeKey(addr);

    // we don't store our own address
    if (key == selfAddr) {
        return;
    }

    // we reject addresses with port 0
    if (addr.port == 0) {
        return;
    }

    // don't add anything in future from clock skew
    uint32_t now = static_cast<uint32_t>(std::time(nullptr));
    if (addr.time > now + 600) return;

    auto it = addresses.find(key);
    if (it != addresses.end()) {
        // if we know it we update timestamp if the new one is more recent
        if (addr.time > it->second.time) {
            it->second.time = addr.time;
        }
        return;
    }

    // we don't add past capacity
    // TODO: evict stale addresses
    if (addresses.size() >= ADDR_MANAGER_MAX_ENTRIES) {
        return;
    }

    addresses[key] = addr;
}

void AddrManager::AddMultiple(const std::vector<NetAddr>& addrs) {
    std::lock_guard<std::mutex> lock(mtx);

    uint32_t now = static_cast<uint32_t>(std::time(nullptr));

    for (const auto& addr : addrs) {
        std::string key = MakeKey(addr);

        if (key == selfAddr) {
            continue;
        }

        if (addr.port == 0) {
            continue;
        }

        // don't add anything in future from clock skew
        if (addr.time > now + 600) {
            continue;
        }

        auto it = addresses.find(key);
        if (it != addresses.end()) {
            if (addr.time > it->second.time) {
                it->second.time = addr.time;
            }
            continue;
        }

        if (addresses.size() >= ADDR_MANAGER_MAX_ENTRIES) {
            break;
        }

        addresses[key] = addr;
    }
}

void AddrManager::MarkGood(const std::string& ip, uint16_t port) {
    std::lock_guard<std::mutex> lock(mtx);

    std::string key = ip + ":" + std::to_string(port);
    auto it = addresses.find(key);
    if (it != addresses.end()) {
        it->second.time = static_cast<uint32_t>(std::time(nullptr));
    }
}

void AddrManager::Remove(const std::string& ip, uint16_t port) {
    std::lock_guard<std::mutex> lock(mtx);
    addresses.erase(ip + ":" + std::to_string(port));
}

std::vector<NetAddr> AddrManager::GetRandomAddresses(
    size_t count, const std::vector<std::string>& excludeAddrs) const {
    std::lock_guard<std::mutex> lock(mtx);

    uint32_t now = static_cast<uint32_t>(std::time(nullptr));

    // we collect all non-stale addresses
    std::vector<const NetAddr*> candidates;
    candidates.reserve(addresses.size());

    for (const auto& [key, addr] : addresses) {
        // skip stale addresses
        if (now - addr.time > ADDR_MAX_AGE_SECS) {
            continue;
        }

        // skip addresses we're already connected to
        bool excluded = false;
        for (const auto& ex : excludeAddrs) {
            if (key == ex) {
                excluded = true;
                break;
            }
        }
        if (excluded) {
            continue;
        }

        candidates.push_back(&addr);
    }

    // shuffle and take up to count
    std::vector<size_t> indices(candidates.size());
    std::iota(indices.begin(), indices.end(), 0);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(indices.begin(), indices.end(), gen);

    std::vector<NetAddr> result;
    size_t resultCount = std::min(count, candidates.size());
    result.reserve(resultCount);

    for (size_t i = 0; i < resultCount; i++) {
        result.push_back(*candidates[indices[i]]);
    }

    return result;
}

std::vector<NetAddr> AddrManager::GetAddressesForGossip() const {
    std::lock_guard<std::mutex> lock(mtx);

    uint32_t now = static_cast<uint32_t>(std::time(nullptr));

    // collect all non-stale addresses
    std::vector<const NetAddr*> candidates;
    candidates.reserve(addresses.size());

    for (const auto& [key, addr] : addresses) {
        if (now - addr.time <= ADDR_MAX_AGE_SECS) {
            candidates.push_back(&addr);
        }
    }

    // shuffle and take up to ADDR_GETADDR_MAX
    std::vector<size_t> indices(candidates.size());
    std::iota(indices.begin(), indices.end(), 0);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(indices.begin(), indices.end(), gen);

    std::vector<NetAddr> result;
    size_t resultCount = std::min(static_cast<size_t>(ADDR_GETADDR_MAX), candidates.size());
    result.reserve(resultCount);

    for (size_t i = 0; i < resultCount; i++) {
        result.push_back(*candidates[indices[i]]);
    }

    return result;
}

size_t AddrManager::Size() const {
    std::lock_guard<std::mutex> lock(mtx);
    return addresses.size();
}

bool AddrManager::Contains(const std::string& ip, uint16_t port) const {
    std::lock_guard<std::mutex> lock(mtx);
    return addresses.count(ip + ":" + std::to_string(port)) > 0;
}