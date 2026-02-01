#ifndef CLI_H
#define CLI_H

#include <string>

#include "blockchain.h"

class CLI {
    private:
        Blockchain* bc;

        void printUsage();
        void addBlock(const std::string& data);
        void printChain();

    public:
        CLI(Blockchain* blockchain);
        void run(int argc, char* argv[]);
};

#endif