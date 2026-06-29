#include "KhutiWallet.h"
#include "KhutiWebServer.h"
#include "NetworkClient.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sys/stat.h>   // For checking directory existence

// Helper: Check if a directory exists (POSIX)
bool directoryExists(const std::string& path) {
    struct stat info;
    if (stat(path.c_str(), &info) != 0) return false;
    return (info.st_mode & S_IFDIR) != 0;
}

// Helper: Extract directory from file path
std::string getDirectory(const std::string& filepath) {
    size_t pos = filepath.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    return filepath.substr(0, pos);
}

int main(int argc, char** argv) {
    std::cout << "🩶 Khuti Hawala Rig v2.0\n";
    std::cout << "========================\n";

    // ----- VAULT LOCATION SELECTOR -----
    std::string wallet_file;
    std::string password = "YourStrongEnigmaPassword"; // CHANGE THIS!

    if (argc > 1) {
        // Use command-line argument
        wallet_file = argv[1];
        std::cout << "🩶 Vault path: " << wallet_file << "\n";
    } else {
        // Interactive prompt
        std::cout << "\n📂 Enter the FULL path where you want to store your vault.\n";
        std::cout << "   (e.g., /sdcard/XXX/khuti_wallet.khuti  or  /storage/emulated/0/MyVault/wallet.khuti)\n";
        std::cout << "   Press ENTER to use default (./khuti_wallet.khuti): ";
        std::getline(std::cin, wallet_file);
        if (wallet_file.empty()) {
            wallet_file = "./khuti_wallet.khuti";
        }
    }

    // Ensure the directory exists; if not, try to create it
    std::string dir = getDirectory(wallet_file);
    if (dir != "." && !directoryExists(dir)) {
        std::cout << "⚠️ Directory does not exist: " << dir << "\n";
        std::cout << "   Please create it first, or choose a valid path.\n";
        std::cout << "   Exiting...\n";
        return 1;
    }

    std::cout << "🩶 Using vault: " << wallet_file << "\n";

    // ----- LOAD OR CREATE VAULT -----
    KhutiWallet wallet;

    if (!wallet.load_from_file(password, wallet_file)) {
        std::cout << "🔐 No vault found at this location. Creating a new one...\n";
        if (!wallet.create_new(password, wallet_file)) {
            std::cerr << "❌ Failed to create vault at: " << wallet_file << "\n";
            std::cerr << "   Check permissions and path.\n";
            return 1;
        }
        std::cout << "✅ New vault created successfully!\n";
    } else {
        std::cout << "✅ Vault loaded successfully!\n";
    }

    // ----- START WEB SERVER -----
    KhutiWebServer web(wallet);
    std::thread web_thread([&]() { web.start(8080); });

    // ----- CONSOLE MENU -----
    std::cout << "\n🩶 Commands: sweep | hawala | balance | vault | path | quit\n";
    std::cout << "   (Web dashboard: http://localhost:8080)\n\n";

    std::string cmd;
    while (std::cin >> cmd) {
        if (cmd == "sweep") {
            wallet.sweep_vintage_utxos();
        }
        else if (cmd == "hawala") {
            std::string recv; double amt;
            std::cout << "Receiver address: "; std::cin >> recv;
            std::cout << "Amount: "; std::cin >> amt;
            uint64_t nonce = std::time(nullptr);
            std::string token = wallet.createHawalaToken(recv, amt, nonce);
            std::cout << "🩶 Hawala Token (give this to receiver):\n" << token << "\n";
        }
        else if (cmd == "balance") {
            std::cout << "On-Chain: " << wallet.getOnChainBalance() << " BTC\n";
            std::cout << "Hawala:   " << wallet.getHawalaPending() << " BTC\n";
        }
        else if (cmd == "vault") {
            // Decrypt and show vault metadata
            std::ifstream file(wallet_file, std::ios::binary);
            if (!file) {
                std::cout << "❌ Vault file not found!\n";
                continue;
            }
            std::vector<unsigned char> encrypted((std::istreambuf_iterator<char>(file)), {});
            file.close();

            KhutiEnigma enigma;
            std::vector<unsigned char> plaintext;
            try {
                plaintext = enigma.decrypt(encrypted, password);
            } catch (const std::exception& e) {
                std::cout << "❌ Failed to decrypt: " << e.what() << "\n";
                continue;
            }

            nlohmann::json vault = nlohmann::json::parse(plaintext);
            std::cout << "\n========================================\n";
            std::cout << "🩶  KHUTI VAULT CONTENTS  🩶\n";
            std::cout << "========================================\n";
            std::cout << "File Path:   " << wallet_file << "\n";
            std::cout << "Address:     " << vault["address"] << "\n";
            std::cout << "Public Key:  " << vault["public_key"].get<std::string>().substr(0, 40) << "...\n";
            std::cout << "Created At:  " << std::ctime((time_t*)&vault["created_at"].get<uint64_t>());
            if (vault.contains("balance_onchain")) {
                std::cout << "On-Chain:    " << vault["balance_onchain"].get<double>() << " BTC\n";
            }
            if (vault.contains("hawala_pending")) {
                std::cout << "Hawala:      " << vault["hawala_pending"].get<double>() << " BTC\n";
            }
            std::cout << "========================================\n\n";
        }
        else if (cmd == "path") {
            std::cout << "📂 Vault file is located at: " << wallet_file << "\n";
            std::cout << "   You can copy this file anywhere.\n";
        }
        else if (cmd == "quit") {
            break;
        } else {
            std::cout << "Commands: sweep | hawala | balance | vault | path | quit\n";
        }
    }

    web_thread.join();
    return 0;
}
