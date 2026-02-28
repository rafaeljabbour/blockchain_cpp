#ifndef CONFIG_H
#define CONFIG_H

#include <string>

inline const std::string DEFAULT_DATA_DIR = "./data";

// minimum fee rate
inline constexpr double MIN_RELAY_FEE_RATE = 0.001;

namespace Config {

// sets the base data directory
void SetDataDir(const std::string& dir);

const std::string& GetDataDir();

std::string GetBlocksPath();
std::string GetWalletPath();

}  // namespace Config

#endif