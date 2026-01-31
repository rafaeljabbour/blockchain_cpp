#include "blockchain.h"
#include <iomanip>
#include <iostream>

int main() {
    Blockchain bc;

    bc.AddBlock("Send 1 BTC for you");
    bc.AddBlock("Send 2 more BTC for you");

    for (const auto& block : bc.GetBlocks()) {
        std::cout << "Previous hash: ";
        for (uint8_t b : block.GetPreviousHash()) printf("%02x", b);
        std::time_t timestamp = block.GetTimestamp();
        std::cout << "\nTimestamp: " << std::ctime(&timestamp);        
        std::cout << "Data: ";
        for (uint8_t b : block.GetData()) std::cout << (char)b;
        std::cout << "\nHash: ";
        for (uint8_t b : block.GetHash()) printf("%02x", b);
        std::cout << "\n\n";
    }
    return 0;
}