#include "KhutiWallet.h"
#include "KhutiWebServer.h"
#include "NetworkClient.h"
#include <iostream>
#include <thread>
#include <chrono>

int main(int argc, char** argv) {
    std::cout << "🩶 Khuti Hawala Rig v2.0\n";
    std::cout << "========================\n";

    std::string password = "YourStrongEnigmaPassword";
    std::string wallet_file = "./khuti_wallet.khuti";

    KhutiWallet wallet;

    // Try load existing, or create new
    if (!wallet.load_from_file(password, wallet_file)) {
        std::cout << "No wallet found, creating new...\n";
        wallet.create_new(password, wallet_file);
    }

    // Start web dashboard
    KhutiWebServer web(wallet);
    std::thread web_thread([&]() { web.start(8080); });

    // NetworkClient (optional: connect to a node for balance updates)
    NetworkClient client;
    // client.connect("127.0.0.1", 8332);

    // Console menu
    std::cout << "\nCommands: sweep | hawala | balance | quit\n";
    std::string cmd;
    while (std::cin >> cmd) {
        if (cmd == "sweep") {
            wallet.sweep_vintage_utxos();
        } else if (cmd == "hawala") {
            std::string recv; double amt;
            std::cout << "Receiver address: "; std::cin >> recv;
            std::cout << "Amount: "; std::cin >> amt;
            uint64_t nonce = std::time(nullptr);
            std::string token = wallet.createHawalaToken(recv, amt, nonce);
            std::cout << "🩶 Hawala Token (give this to receiver):\n" << token << "\n";
        } else if (cmd == "balance") {
            std::cout << "On-Chain: " << wallet.getOnChainBalance() << " BTC\n";
            std::cout << "Hawala:   " << wallet.getHawalaPending() << " BTC\n";
        } else if (cmd == "quit") {
            break;
        }
    }

    web_thread.join();
    return 0;
}
