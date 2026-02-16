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

std::string GetWalletPath() { return (std::filesystem::path(dataDir) / "wallet.dat").string(); }

}  // namespace Config