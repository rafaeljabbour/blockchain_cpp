#include "blockchain.h"
#include "cli.h"

int main(int argc, char* argv[]) {
    CLI cli();
    cli.run(argc, argv);

    return 0;
}