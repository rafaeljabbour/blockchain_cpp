#include "blockchain.h"
#include "cli.h"

int main(int argc, char* argv[]) {
    Blockchain bc;
    CLI cli(&bc);
    cli.run(argc, argv);

    return 0;
}