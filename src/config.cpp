#include "config.h"

#include <filesystem>
#include <stdexcept>

namespace Config {

    // internal storage for the data directory
    static std::string dataDir = DEFAULT_DATA_DIR;

    void SetDataDir(const std::string& dir) {
        if (dir.empty()) {
            throw std::invalid_argument("Data directory cannot be empty");
        }
        dataDir = dir;
    }

    const std::string& GetDataDir() { return dataDir; }

    std::string GetBlocksPath() { return (std::filesystem::path(dataDir) / "blocks").string(); }

    std::string GetUTXOPath() { return (std::filesystem::path(dataDir) / "utxo").string(); }

    std::string GetWalletPath() { return (std::filesystem::path(dataDir) / "wallet.dat").string(); }

    std::string GetPeersPath() { return (std::filesystem::path(dataDir) / "peers.dat").string(); }

    std::string GetBanListPath() { return (std::filesystem::path(dataDir) / "banlist.dat").string(); }

}  // namespace Config