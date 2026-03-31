#include "addrManager.h"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>

#include "config.h"
#include "serialization.h"

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

    // if we are at capacity, we evict the oldest address to make room
    if (addresses.size() >= ADDR_MANAGER_MAX_ENTRIES) {
        auto oldest = addresses.begin();
        for (auto it2 = addresses.begin(); it2 != addresses.end(); ++it2) {
            if (it2->second.time < oldest->second.time) {
                oldest = it2;
            }
        }
        addresses.erase(oldest);
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
            auto oldest = addresses.begin();
            for (auto it2 = addresses.begin(); it2 != addresses.end(); ++it2) {
                if (it2->second.time < oldest->second.time) {
                    oldest = it2;
                }
            }
            addresses.erase(oldest);
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

void AddrManager::SaveToFile() const {
    std::lock_guard<std::mutex> lock(mtx);

    std::string path = Config::GetPeersPath();
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());

    // format: [count(4)] [NetAddr with time]
    std::vector<uint8_t> data;
    WriteUint32(data, static_cast<uint32_t>(addresses.size()));

    for (const auto& [key, addr] : addresses) {
        std::vector<uint8_t> serialized = addr.Serialize(/*includeTime=*/true);
        data.insert(data.end(), serialized.begin(), serialized.end());
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::cerr << "[addrmanager] Failed to open " << path << " for writing" << std::endl;
        return;
    }

    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
    std::cout << "[addrmanager] Saved " << addresses.size() << " addresses to " << path
              << std::endl;
}

void AddrManager::LoadFromFile() {
    std::lock_guard<std::mutex> lock(mtx);

    std::string path = Config::GetPeersPath();
    if (!std::filesystem::exists(path)) {
        return;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "[addrmanager] Failed to open " << path << " for reading" << std::endl;
        return;
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());

    if (data.size() < 4) {
        std::cerr << "[addrmanager] Peers file too small, ignoring" << std::endl;
        return;
    }

    uint32_t count = ReadUint32(data, 0);
    size_t offset = 4;

    // cap to prevent corrupted file from allocating huge amounts
    if (count > ADDR_MANAGER_MAX_ENTRIES) {
        count = ADDR_MANAGER_MAX_ENTRIES;
    }

    size_t loaded = 0;
    for (uint32_t i = 0; i < count; ++i) {
        try {
            auto [addr, bytesRead] = NetAddr::Deserialize(data, offset, /*includeTime=*/true);
            offset += bytesRead;

            std::string key = MakeKey(addr);
            if (key == selfAddr || addr.port == 0) continue;

            if (addresses.size() < ADDR_MANAGER_MAX_ENTRIES) {
                addresses[key] = addr;
                loaded++;
            }
        } catch (const std::exception&) {
            // means it's a corrupted entry so we stop reading
            break;
        }
    }

    std::cout << "[addrmanager] Loaded " << loaded << " addresses from " << path << std::endl;
}