#include "cli.h"

#include <cstdio>
#include <ctime>
#include <iostream>

#include "blockchain.h"
#include "blockchainIterator.h"
#include "proofOfWork.h"
#include "utils.h"

void CLI::printUsage() {
    std::cout << "Usage:\n";
    std::cout << "  createblockchain -address ADDRESS - Create a blockchain and send genesis block "
                 "reward to ADDRESS\n";
    std::cout << "  getbalance -address ADDRESS - Get balance of ADDRESS\n";
    std::cout << "  printchain - Print all the blocks of the blockchain\n";
    std::cout << "  send -from FROM -to TO -amount AMOUNT - Send AMOUNT of coins from FROM address "
                 "to TO\n";
}

void CLI::createBlockchain(const std::string& address) {
    auto bc = Blockchain::CreateBlockchain(address);
    std::cout << "Done!" << std::endl;
}

void CLI::getBalance(const std::string& address) {
    Blockchain bc;

    int balance = 0;
    std::vector<TransactionOutput> UTXOs = bc.FindUTXO(address);

    for (const TransactionOutput& out : UTXOs) {
        balance += out.GetValue();
    }

    std::cout << "Balance of '" << address << "': " << balance << std::endl;
}

void CLI::send(const std::string& from, const std::string& to, int amount) {
    Blockchain bc;

    Transaction tx = Transaction::NewUTXOTransaction(from, to, amount, &bc);
    bc.MineBlock({tx});

    std::cout << "Success!" << std::endl;
}

void CLI::run(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return;
    }

    std::string command = argv[1];

    if (command == "createblockchain") {
        if (argc < 4 || std::string(argv[2]) != "-address") {
            std::cout << "Error: createblockchain requires -address flag\n";
            printUsage();
            return;
        }
        std::string address = argv[3];
        createBlockchain(address);
    } else if (command == "getbalance") {
        if (argc < 4 || std::string(argv[2]) != "-address") {
            std::cout << "Error: getbalance requires -address flag\n";
            printUsage();
            return;
        }
        std::string address = argv[3];
        getBalance(address);
    } else if (command == "printchain") {
        printChain();
    } else if (command == "send") {
        if (argc < 8) {
            std::cout << "Error: send requires -from, -to, and -amount flags\n";
            printUsage();
            return;
        }

        std::string from, to;
        int amount = 0;

        // Parse flags
        for (int i = 2; i < argc; i += 2) {
            std::string flag = argv[i];
            if (i + 1 >= argc) {
                std::cout << "Error: flag " << flag << " requires a value\n";
                printUsage();
                return;
            }

            if (flag == "-from") {
                from = argv[i + 1];
            } else if (flag == "-to") {
                to = argv[i + 1];
            } else if (flag == "-amount") {
                amount = std::stoi(argv[i + 1]);
            } else {
                std::cout << "Error: unknown flag " << flag << "\n";
                printUsage();
                return;
            }
        }

        if (from.empty() || to.empty() || amount <= 0) {
            std::cout << "Error: -from, -to, and -amount are required and amount must be > 0\n";
            printUsage();
            return;
        }

        send(from, to, amount);
    } else {
        std::cout << "Error: unknown command '" << command << "'\n";
        printUsage();
    }
}

void CLI::printChain() {
    Blockchain bc;
    BlockchainIterator bci = bc.Iterator();

    while (bci.hasNext()) {
        Block block = bci.Next();

        std::cout << "Block:" << std::endl;
        std::cout << "\tPrev. hash: " << ByteArrayToHexString(block.GetPreviousHash()) << std::endl;
        std::cout << "\tHash: " << ByteArrayToHexString(block.GetHash()) << std::endl;
        std::cout << "\tTransactions: " << block.GetTransactions().size() << std::endl;

        ProofOfWork pow(&block);
        std::cout << "\tPoW: " << std::boolalpha << pow.Validate() << std::endl;
        std::cout << std::endl;
    }
}