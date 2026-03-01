#ifndef CONFIG_H
#define CONFIG_H

#include <climits>
#include <cstdint>
#include <string>

inline const std::string DEFAULT_DATA_DIR = "./data";

// consensus parameters
namespace Consensus {

    // block reward
    inline constexpr int64_t SUBSIDY = 10;
    inline constexpr int32_t HALVING_INTERVAL = 210'000;

    // halving every HALVING_INTERVAL blocks
    inline constexpr int64_t GetBlockSubsidy(int32_t height) {
        int halvings = height / HALVING_INTERVAL;
        if (halvings >= 64) return 0;
        return SUBSIDY >> halvings;
    }

    // proof-of-work
    inline constexpr int32_t INITIAL_BITS = 17;
    inline constexpr int32_t RETARGET_INTERVAL = 2016;
    inline constexpr int64_t TARGET_TIMESPAN = 2016LL * 10 * 60;  // 1 209 600 s
    inline constexpr int32_t MIN_BITS = 1;                        // easiest target
    inline constexpr int32_t MAX_BITS = 255;                      // hardest target

    // genesis block
    inline const std::string GENESIS_COINBASE_DATA =
        "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";

}  // namespace Consensus

// network policy
namespace Policy {

    inline constexpr uint32_t MAX_BLOCK_SIZE = 1'000'000;  // 1 MB
    inline constexpr uint32_t MAX_BLOCK_TXS = 5'000;       // sanity cap
    inline constexpr double MIN_RELAY_FEE_RATE = 0.001;    // per serialized byte

}  // namespace Policy

// data directory
namespace Config {

    void SetDataDir(const std::string& dir);

    const std::string& GetDataDir();

    std::string GetBlocksPath();
    std::string GetWalletPath();

}  // namespace Config

#endif