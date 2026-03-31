#ifndef BANMANAGER_H
#define BANMANAGER_H

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

// we ban for a duration of 24 hours
inline constexpr int64_t DEFAULT_BAN_DURATION_SECS = 24 * 60 * 60;

// misbehavior score threshold that triggers an automatic ban
inline constexpr int32_t BAN_SCORE_THRESHOLD = 100;

struct BanEntry {
        int64_t banUntil;  // the unix timestamp indicating when the ban expires, we can make it 0 to have it permanent
        int64_t createTime;
        std::string reason;
};

// manages per-IP bans, and stores it data/banlist.dat
class BanManager {
    private:
        mutable std::mutex mtx;
        std::unordered_map<std::string, BanEntry> banMap;

    public:
        BanManager() = default;
        ~BanManager() = default;

        BanManager(const BanManager&) = delete;
        BanManager& operator=(const BanManager&) = delete;

        // ban an IP for a duration, if it's set to 0 it's permanent
        void Ban(const std::string& ip, const std::string& reason,
                 int64_t durationSecs = DEFAULT_BAN_DURATION_SECS);

        // check if an IP is currently banned
        bool IsBanned(const std::string& ip) const;

        // remove a ban manually
        void Unban(const std::string& ip);

        // remove all expired bans
        void SweepExpired();

        size_t Size() const;

        void SaveToFile() const;
        void LoadFromFile();
};

#endif
