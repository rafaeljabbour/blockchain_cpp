#ifndef CLI_H
#define CLI_H

#include <string>

#include "blockchain.h"

class CLI {
    private:
        void printUsage();

        void createBlockchain(const std::string& address);
        void getBalance(const std::string& address);
        void printChain();
        void send(const std::string& from, const std::string& to, int amount);

    public:
        CLI() = default;
        void run(int argc, char* argv[]);
};

#endif