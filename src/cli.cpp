#include "cli.h"

#include <ctime>
#include <iostream>
#include <stdexcept>

#include "base58.h"
#include "blockchain.h"
#include "blockchainIterator.h"
#include "config.h"
#include "node.h"
#include "proofOfWork.h"
#include "serialization.h"
#include "utxoSet.h"
#include "wallet.h"
#include "wallets.h"

void CLI::printUsage() {
    std::cout << "Usage:\n";
    std::cout << "  createwallet - Generate a new wallet and get its address\n";
    std::cout << "  createblockchain -address ADDRESS - Create a blockchain and send genesis block "
                 "reward to ADDRESS\n";
    std::cout << "  getbalance -address ADDRESS - Get balance of ADDRESS\n";
    std::cout << "  listaddresses - List all addresses from the wallet file\n";
    std::cout << "  printchain - Print all the blocks of the blockchain\n";
    std::cout << "  reindexutxo - Rebuilds the UTXO set\n";
    std::cout << "  send -from FROM -to TO -amount AMOUNT - Send AMOUNT of coins from FROM address "
                 "to TO\n";
    std::cout << "  startnode -port PORT [-seed IP:PORT] [-rpcport PORT] [-mine -mineraddress ADDR]"
                 " - Start a network node\n";
    std::cout << "\nGlobal flags:\n";
    std::cout << "  -datadir DIR - Set the data directory (default: ./data)\n";
}

void CLI::createBlockchain(const std::string& address) {
    if (!Wallet::ValidateAddress(address)) {
        throw std::runtime_error("Invalid address");
    }

    auto bc = Blockchain::CreateBlockchain(address);

    UTXOSet utxoSet(bc.get());
    utxoSet.Reindex();

    std::cout << "Done! There are " << utxoSet.CountTransactions()
              << " transactions in the UTXO set." << std::endl;
}

void CLI::createWallet() {
    Wallets wallets;
    std::string address = wallets.CreateWallet();
    wallets.SaveToFile();

    std::cout << "Your new address: " << address << std::endl;
}

void CLI::getBalance(const std::string& address) {
    if (!Wallet::ValidateAddress(address)) {
        throw std::runtime_error("Invalid address");
    }

    Blockchain bc;
    UTXOSet utxoSet(&bc);

    // decode address to get pubKeyHash
    std::vector<uint8_t> decoded = Base58DecodeStr(address);
    std::vector<uint8_t> pubKeyHash(decoded.begin() + 1, decoded.end() - ADDRESS_CHECKSUM_LEN);

    int balance = 0;
    std::vector<TransactionOutput> UTXOs = utxoSet.FindUTXO(pubKeyHash);

    for (const TransactionOutput& out : UTXOs) {
        balance += out.GetValue();
    }

    std::cout << "Balance of '" << address << "': " << balance << std::endl;
}

void CLI::reindexUTXO() {
    Blockchain bc;
    UTXOSet utxoSet(&bc);
    utxoSet.Reindex();

    int count = utxoSet.CountTransactions();
    std::cout << "Done! There are " << count << " transactions in the UTXO set." << std::endl;
}

void CLI::listAddresses() {
    Wallets wallets;
    std::vector<std::string> addresses = wallets.GetAddresses();

    if (addresses.empty()) {
        std::cout << "No wallets found. Create one with 'createwallet' command." << std::endl;
        return;
    }

    std::cout << "Addresses:" << std::endl;
    for (const std::string& address : addresses) {
        std::cout << "  " << address << std::endl;
    }
}

void CLI::send(const std::string& from, const std::string& to, int amount) {
    if (!Wallet::ValidateAddress(from)) {
        throw std::runtime_error("Invalid sender address");
    }

    if (!Wallet::ValidateAddress(to)) {
        throw std::runtime_error("Invalid recipient address");
    }

    Blockchain bc;
    UTXOSet utxoSet(&bc);

    // create the transaction
    Transaction tx = Transaction::NewUTXOTransaction(from, to, amount, &utxoSet);

    // mining reward for the sender
    Transaction coinbaseTx = Transaction::NewCoinbaseTX(from, "");

    // mine the block with both transactions
    std::vector<Transaction> txs = {coinbaseTx, tx};
    Block newBlock = bc.MineBlock(txs);

    utxoSet.Update(newBlock);

    std::cout << "Success!" << std::endl;
}

void CLI::startNode(uint16_t port, const std::string& seedAddr, uint16_t rpcPort,
                    const std::string& minerAddress) {
    Node node("0.0.0.0", port, rpcPort, minerAddress);

    // handles seed connection and then enters the accept loop
    node.Start(seedAddr);
}

void CLI::run(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return;
    }

    // parse global -datadir flag
    int cmdStart = 1;
    if (std::string(argv[1]) == "-datadir") {
        if (argc < 4) {
            std::cout << "Error: -datadir requires a value\n";
            printUsage();
            return;
        }
        Config::SetDataDir(argv[2]);
        cmdStart = 3;
    }

    if (cmdStart >= argc) {
        printUsage();
        return;
    }

    // shift argv
    int cmdArgc = argc - cmdStart;
    char** cmdArgv = argv + cmdStart;

    std::string command = cmdArgv[0];

    if (command == "createwallet") {
        createWallet();
    } else if (command == "createblockchain") {
        if (cmdArgc < 3 || std::string(cmdArgv[1]) != "-address") {
            std::cout << "Error: createblockchain requires -address flag\n";
            printUsage();
            return;
        }
        std::string address = cmdArgv[2];
        createBlockchain(address);
    } else if (command == "getbalance") {
        if (cmdArgc < 3 || std::string(cmdArgv[1]) != "-address") {
            std::cout << "Error: getbalance requires -address flag\n";
            printUsage();
            return;
        }
        std::string address = cmdArgv[2];
        getBalance(address);
    } else if (command == "listaddresses") {
        listAddresses();
    } else if (command == "printchain") {
        printChain();
    } else if (command == "reindexutxo") {
        reindexUTXO();
    } else if (command == "send") {
        if (cmdArgc < 7) {
            std::cout << "Error: send requires -from, -to, and -amount flags\n";
            printUsage();
            return;
        }

        std::string from, to;
        int amount = 0;

        // parse flags
        for (int i = 1; i < cmdArgc; i += 2) {
            std::string flag = cmdArgv[i];
            if (i + 1 >= cmdArgc) {
                std::cout << "Error: flag " << flag << " requires a value\n";
                printUsage();
                return;
            }

            if (flag == "-from") {
                from = cmdArgv[i + 1];
            } else if (flag == "-to") {
                to = cmdArgv[i + 1];
            } else if (flag == "-amount") {
                amount = std::stoi(cmdArgv[i + 1]);
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
    } else if (command == "startnode") {
        if (cmdArgc < 3 || std::string(cmdArgv[1]) != "-port") {
            std::cout << "Error: startnode requires -port flag\n";
            printUsage();
            return;
        }

        uint16_t port = static_cast<uint16_t>(std::stoi(cmdArgv[2]));
        std::string seedAddr;
        uint16_t rpcPort = DEFAULT_RPC_PORT;
        std::string minerAddress;
        bool mineEnabled = false;

        // parse optional flags
        for (int i = 3; i < cmdArgc; i++) {
            std::string flag = cmdArgv[i];
            if (flag == "-mine") {
                mineEnabled = true;
            } else if (i + 1 < cmdArgc) {
                if (flag == "-seed") {
                    seedAddr = cmdArgv[++i];
                } else if (flag == "-rpcport") {
                    rpcPort = static_cast<uint16_t>(std::stoi(cmdArgv[++i]));
                } else if (flag == "-mineraddress") {
                    minerAddress = cmdArgv[++i];
                }
            }
        }

        if (mineEnabled && minerAddress.empty()) {
            std::cout << "Error: -mine requires -mineraddress ADDRESS\n";
            printUsage();
            return;
        }

        startNode(port, seedAddr, rpcPort, minerAddress);
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

        std::cout << "Block: " << ByteArrayToHexString(block.GetHash()) << std::endl;
        std::cout << "Prev. block: " << ByteArrayToHexString(block.GetPreviousHash()) << std::endl;
        std::cout << "Bits: " << block.GetBits() << "  (target = 1 << " << (256 - block.GetBits())
                  << ")" << std::endl;

        ProofOfWork pow(&block);
        std::cout << "PoW valid: " << std::boolalpha << pow.Validate() << std::endl;
        std::cout << std::endl;

        // print each transaction on that block
        for (const Transaction& tx : block.GetTransactions()) {
            std::cout << "--- Transaction " << ByteArrayToHexString(tx.GetID()) << ":" << std::endl;

            if (tx.IsCoinbase()) {
                std::cout << "\tCOINBASE" << std::endl;
            } else {
                std::cout << "\tInputs:" << std::endl;
                for (const auto& input : tx.GetVin()) {
                    std::cout << "\t\tTxID: " << ByteArrayToHexString(input.GetTxid()) << std::endl;
                    std::cout << "\t\tVout: " << input.GetVout() << std::endl;
                }
            }

            std::cout << "\tOutputs:" << std::endl;
            for (size_t i = 0; i < tx.GetVout().size(); i++) {
                const auto& output = tx.GetVout()[i];
                std::cout << "\t\tOutput " << i << ":" << std::endl;
                std::cout << "\t\t\tValue: " << output.GetValue() << std::endl;
                std::cout << "\t\t\tPubKeyHash: " << ByteArrayToHexString(output.GetPubKeyHash())
                          << std::endl;
            }
            std::cout << std::endl;
        }

        std::cout << std::endl;
    }
}