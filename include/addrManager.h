#ifndef ADDRMANAGER_H
#define ADDRMANAGER_H

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "netAddr.h"

// we exclude stale addresses, older than max age
inline constexpr uint32_t ADDR_MAX_AGE_SECS = 3 * 60 * 60;  // 3 hours

// maximum number of addresses we store
inline constexpr size_t ADDR_MANAGER_MAX_ENTRIES = 2500;

// how many addresses we return in response to a getaddr request
inline constexpr size_t ADDR_GETADDR_MAX = 1000;

// we stores known peer addresses with key information about them
class AddrManager {
    private:
        mutable std::mutex mtx;

        // keyed by "ip:port" string to prevent duplicates
        std::unordered_map<std::string, NetAddr> addresses;

        // we prevent storing our own listening address
        std::string selfAddr;

        // returns "ip:port" key for a NetAddr
        static std::string MakeKey(const NetAddr& addr);

        // we extracts IPv4 string from a NetAddr's mapped IPv6 field
        static std::string ExtractIPv4(const NetAddr& addr);

    public:
        AddrManager() = default;
        ~AddrManager() = default;

        // prevent copying
        AddrManager(const AddrManager&) = delete;
        AddrManager& operator=(const AddrManager&) = delete;

        void SetSelfAddr(const std::string& ip, uint16_t port);
        void Add(const NetAddr& addr);
        void AddMultiple(const std::vector<NetAddr>& addrs);
        void MarkGood(const std::string& ip, uint16_t port);
        void Remove(const std::string& ip, uint16_t port);
        std::vector<NetAddr> GetRandomAddresses(size_t count,
                                                const std::vector<std::string>& excludeAddrs) const;
        std::vector<NetAddr> GetAddressesForGossip() const;
        size_t Size() const;
        bool Contains(const std::string& ip, uint16_t port) const;

        // persist address book to disk
        void SaveToFile() const;
        void LoadFromFile();
};

#endif