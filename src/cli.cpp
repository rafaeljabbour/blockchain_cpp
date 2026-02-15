#include "cli.h"

#include <ctime>
#include <iostream>

#include "base58.h"
#include "blockchain.h"
#include "blockchainIterator.h"
#include "proofOfWork.h"
#include "utils.h"
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

void CLI::run(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return;
    }

    std::string command = argv[1];

    if (command == "createwallet") {
        createWallet();
    } else if (command == "createblockchain") {
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
    } else if (command == "listaddresses") {
        listAddresses();
    } else if (command == "printchain") {
        printChain();
    } else if (command == "reindexutxo") {
        reindexUTXO();
    } else if (command == "send") {
        if (argc < 8) {
            std::cout << "Error: send requires -from, -to, and -amount flags\n";
            printUsage();
            return;
        }

        std::string from, to;
        int amount = 0;

        // parse flags
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

        std::cout << "Block " << ByteArrayToHexString(block.GetHash()) << std::endl;
        std::cout << "Prev. block: " << ByteArrayToHexString(block.GetPreviousHash()) << std::endl;

        ProofOfWork pow(&block);
        std::cout << "PoW: " << std::boolalpha << pow.Validate() << std::endl;
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