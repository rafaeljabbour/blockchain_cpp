#include <iostream>

#include "cli.h"

int main(int argc, char* argv[]) {
    try {
        CLI cli;
        cli.run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}