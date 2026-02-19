#include <cstdint>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "rpcServer.h"

using json = nlohmann::json;

void printUsage() {
    std::cout << "Usage: blockchain-rpc [options] <method> [method-flags]\n";
    std::cout << "\nSends a JSON-RPC request to a running node and prints the result.\n";
    std::cout << "\nOptions:\n";
    std::cout << "  -rpcport PORT  JSON-RPC port to connect to (default: " << DEFAULT_RPC_PORT
              << ")\n";
    std::cout << "\nMethods (no flags):\n";
    std::cout << "  getmempool      - list unconfirmed transactions\n";
    std::cout << "  getblockcount   - current chain height\n";
    std::cout << "  getsyncing      - sync status\n";
    std::cout << "\nMethods with flags:\n";
    std::cout << "  sendtx -from ADDR -to ADDR -amount N\n";
    std::cout << "          build a transaction from a wallet address and submit to the\n";
    std::cout << "          node's mempool; the node relays it to peers\n";
    std::cout << "  mine -address ADDR\n";
    std::cout << "          mine one block from the current mempool and give the coinbase\n";
    std::cout << "          reward to ADDR; the block is broadcast to all peers\n";
    std::cout << "\nExamples:\n";
    std::cout << "  blockchain-rpc getmempool\n";
    std::cout << "  blockchain-rpc -rpcport 9335 getmempool\n";
    std::cout << "  blockchain-rpc sendtx -from ADDR1 -to ADDR2 -amount 5\n";
    std::cout << "  blockchain-rpc mine -address ADDR1\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    uint16_t rpcPort = DEFAULT_RPC_PORT;
    std::string method;
    int methodIdx = -1;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-rpcport") {
            if (i + 1 >= argc) {
                std::cerr << "Error: -rpcport requires a value\n";
                return 1;
            }
            rpcPort = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "-h" || arg == "--help") {
            printUsage();
            return 0;
        } else {
            method = arg;
            methodIdx = i;
            break;
        }
    }

    if (method.empty()) {
        std::cerr << "Error: no method specified\n";
        printUsage();
        return 1;
    }

    // build params from the flags of each method
    json params = json::object();

    if (method == "sendtx") {
        for (int i = methodIdx + 1; i < argc; i += 2) {
            if (i + 1 >= argc) {
                std::cerr << "Error: flag " << argv[i] << " requires a value\n";
                return 1;
            }
            std::string flag = argv[i];
            if (flag == "-from") {
                params["from"] = argv[i + 1];
            } else if (flag == "-to") {
                params["to"] = argv[i + 1];
            } else if (flag == "-amount") {
                params["amount"] = std::stoi(argv[i + 1]);
            } else {
                std::cerr << "Error: unknown flag '" << flag << "' for sendtx\n";
                return 1;
            }
        }
        if (!params.contains("from") || !params.contains("to") || !params.contains("amount")) {
            std::cerr << "Error: sendtx requires -from, -to, and -amount\n";
            return 1;
        }
    } else if (method == "mine") {
        for (int i = methodIdx + 1; i < argc; i += 2) {
            if (i + 1 >= argc) {
                std::cerr << "Error: flag " << argv[i] << " requires a value\n";
                return 1;
            }
            std::string flag = argv[i];
            if (flag == "-address") {
                params["address"] = argv[i + 1];
            } else {
                std::cerr << "Error: unknown flag '" << flag << "' for mine\n";
                return 1;
            }
        }
        if (!params.contains("address")) {
            std::cerr << "Error: mine requires -address\n";
            return 1;
        }
    }

    try {
        json result = RPCCall(rpcPort, method, params);

        if (result.is_string()) {
            std::cout << result.get<std::string>() << std::endl;
        } else {
            std::cout << result.dump(2) << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Is the node running with JSON-RPC on port " << rpcPort << "?" << std::endl;
        return 1;
    }

    return 0;
}