#include "banManager.h"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "config.h"
#include "serialization.h"

void BanManager::Ban(const std::string& ip, const std::string& reason, int64_t durationSecs) {
    std::lock_guard<std::mutex> lock(mtx);

    int64_t now = std::time(nullptr);
    BanEntry entry;
    entry.createTime = now;
    entry.banUntil = (durationSecs > 0) ? now + durationSecs : 0;
    entry.reason = reason;

    banMap[ip] = entry;
    std::cout << "[ban] Banned " << ip << " for " << durationSecs << "s: " << reason << std::endl;
}

bool BanManager::IsBanned(const std::string& ip) const {
    std::lock_guard<std::mutex> lock(mtx);

    auto it = banMap.find(ip);
    if (it == banMap.end()) return false;

    // permanent ban
    if (it->second.banUntil == 0) return true;

    // check expiry
    int64_t now = std::time(nullptr);
    return now < it->second.banUntil;
}

void BanManager::Unban(const std::string& ip) {
    std::lock_guard<std::mutex> lock(mtx);
    banMap.erase(ip);
}

void BanManager::SweepExpired() {
    std::lock_guard<std::mutex> lock(mtx);

    int64_t now = std::time(nullptr);
    for (auto it = banMap.begin(); it != banMap.end();) {
        // don't unban permanent bans and non-expired bans
        if (it->second.banUntil != 0 && now >= it->second.banUntil) {
            it = banMap.erase(it);
        } else {
            ++it;
        }
    }
}

size_t BanManager::Size() const {
    std::lock_guard<std::mutex> lock(mtx);
    return banMap.size();
}

void BanManager::SaveToFile() const {
    std::lock_guard<std::mutex> lock(mtx);

    std::string path = Config::GetBanListPath();
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());

    // format: [count(4)] [ip_len(4) ip_bytes banUntil(8) createTime(8) reason_len(4) reason_bytes]
    std::vector<uint8_t> data;
    WriteUint32(data, static_cast<uint32_t>(banMap.size()));

    for (const auto& [ip, entry] : banMap) {
        WriteUint32(data, static_cast<uint32_t>(ip.size()));
        data.insert(data.end(), ip.begin(), ip.end());

        WriteUint64(data, static_cast<uint64_t>(entry.banUntil));
        WriteUint64(data, static_cast<uint64_t>(entry.createTime));

        WriteUint32(data, static_cast<uint32_t>(entry.reason.size()));
        data.insert(data.end(), entry.reason.begin(), entry.reason.end());
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::cerr << "[ban] Failed to open " << path << " for writing" << std::endl;
        return;
    }

    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
    std::cout << "[ban] Saved " << banMap.size() << " ban entries to " << path << std::endl;
}

void BanManager::LoadFromFile() {
    std::lock_guard<std::mutex> lock(mtx);

    std::string path = Config::GetBanListPath();
    if (!std::filesystem::exists(path)) return;

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "[ban] Failed to open " << path << " for reading" << std::endl;
        return;
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());

    if (data.size() < 4) return;

    uint32_t count = ReadUint32(data, 0);
    size_t offset = 4;

    // cap to prevent corrupted file from allocating huge amounts
    if (count > 100'000) count = 100'000;

    size_t loaded = 0;
    for (uint32_t i = 0; i < count; ++i) {
        try {
            if (offset + 4 > data.size()) break;
            uint32_t ipLen = ReadUint32(data, offset);
            offset += 4;

            if (ipLen > 256 || offset + ipLen > data.size()) break;
            std::string ip(data.begin() + offset, data.begin() + offset + ipLen);
            offset += ipLen;

            if (offset + 16 > data.size()) break;
            int64_t banUntil = static_cast<int64_t>(ReadUint64(data, offset));
            offset += 8;
            int64_t createTime = static_cast<int64_t>(ReadUint64(data, offset));
            offset += 8;

            if (offset + 4 > data.size()) break;
            uint32_t reasonLen = ReadUint32(data, offset);
            offset += 4;

            if (reasonLen > 1024 || offset + reasonLen > data.size()) break;
            std::string reason(data.begin() + offset, data.begin() + offset + reasonLen);
            offset += reasonLen;

            BanEntry entry;
            entry.banUntil = banUntil;
            entry.createTime = createTime;
            entry.reason = reason;
            banMap[ip] = entry;
            loaded++;
        } catch (const std::exception&) {
            break;
        }
    }

    std::cout << "[ban] Loaded " << loaded << " ban entries from " << path << std::endl;
}
