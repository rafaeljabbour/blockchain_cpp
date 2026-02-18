#include <cstdint>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "rpcServer.h"

using json = nlohmann::json;

void printUsage() {
    std::cout << "Usage: blockchain-rpc [options] <method>\n";
    std::cout << "\nSends a JSON-RPC request to a running node and prints the result.\n";
    std::cout << "Any registered RPC method can be called.\n";
    std::cout << "\nOptions:\n";
    std::cout << "  -rpcport PORT - JSON-RPC port to connect to (default: " << DEFAULT_RPC_PORT
              << ")\n";
    std::cout << "\nExamples:\n";
    std::cout << "  blockchain-rpc getmempool\n";
    std::cout << "  blockchain-rpc -rpcport 9335 getmempool\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    uint16_t rpcPort = DEFAULT_RPC_PORT;
    std::string method;

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
        }
    }

    if (method.empty()) {
        std::cerr << "Error: no method specified\n";
        printUsage();
        return 1;
    }

    try {
        json result = RPCCall(rpcPort, method);

        // print whatever comes back
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