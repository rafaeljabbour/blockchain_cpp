#include "cli.h"
#include "blockchain.h"
#include "blockchainIterator.h"
#include "proofOfWork.h"

#include <iostream>
#include <cstdio>
#include <ctime>  

CLI::CLI(Blockchain* blockchain) : bc(blockchain) {}

void CLI::run(int argc, char* argv[]){
    if (argc < 2) {
        printUsage();
        return;
    }
    
    std::string command = argv[1];
    
    if (command == "addblock") {
        if (argc < 4 || std::string(argv[2]) != "-data"){
            std::cout << "Error: addblock requires -data flag\n";
            printUsage();
            return;
        }
        std::string data = argv[3];
        addBlock(data);
    } else if (command == "printchain") {
        printChain();
    }
    else{
        printUsage();
        return;
    }
}

void CLI::printUsage() {
    std::cout << "Usage:\n";
    std::cout << "\taddblock -data BLOCK_DATA - add a block to the blockchain\n";
    std::cout << "\tprintchain                - print all blocks in the blockchain\n";
}

void CLI::addBlock(const std::string& data) {
    bc->AddBlock(data);
    std::cout << "Block has been added!" << std::endl;
}

void CLI::printChain(){
    BlockchainIterator bcIterator = bc->Iterator();

    while (bcIterator.hasNext()) {
        Block block = bcIterator.Next();

        std::cout << "Previous hash: ";
        for (uint8_t b : block.GetPreviousHash()) printf("%02x", b);

        std::time_t timestamp = block.GetTimestamp();
        std::cout << "\nTimestamp: " << std::ctime(&timestamp);        
        std::cout << "Data: ";
        for (uint8_t b : block.GetData()) std::cout << (char)b;
        std::cout << "\nHash: ";
        for (uint8_t b : block.GetHash()) printf("%02x", b);

        ProofOfWork proofOfWork(&block);
        std::cout << "\nValid: " << (proofOfWork.Validate() ? "true" : "false");
        std::cout << "\n\n";
    }
}