#include "KhutiWallet.h"
#include "KhutiEnigma.h"
#include "NetworkClient.h"
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <openssl/bn.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>

// -- Base58 & address helpers (simplified) --
static const std::string BASE58_ALPH = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

std::string KhutiWallet::base58_encode(const std::vector<unsigned char>& data) {
    std::vector<unsigned char> digits(data.size() * 2);
    int len = 0;
    for (auto byte : data) {
        int carry = byte;
        for (int i = 0; i < len; i++) {
            carry += digits[i] << 8;
            digits[i] = carry % 58;
            carry /= 58;
        }
        while (carry) {
            digits[len++] = carry % 58;
            carry /= 58;
        }
    }
    std::string result;
    for (int i = len - 1; i >= 0; i--) result += BASE58_ALPH[digits[i]];
    for (auto c : data) if (c == 0) result = BASE58_ALPH[0] + result; else break;
    return result;
}

std::vector<unsigned char> KhutiWallet::base58_decode(const std::string& str) {
    std::vector<unsigned char> result(str.size() * 2);
    int len = 0;
    for (char c : str) {
        int val = BASE58_ALPH.find(c);
        if (val < 0) continue;
        int carry = val;
        for (int i = 0; i < len; i++) {
            carry += result[i] * 58;
            result[i] = carry & 0xFF;
            carry >>= 8;
        }
        while (carry) {
            result[len++] = carry & 0xFF;
            carry >>= 8;
        }
    }
    result.resize(len);
    std::reverse(result.begin(), result.end());
    return result;
}

// -- Wallet Implementation --
KhutiWallet::KhutiWallet() : onchain_balance(0), hawala_pending_balance(0),
                             last_block_height(0), is_loaded(false) {
    memset(private_key, 0, 32);
}

void KhutiWallet::generate_keypair() {
    RAND_bytes(private_key, 32);
    // Derive public key (OpenSSL EC)
    EC_KEY* key = EC_KEY_new_by_curve_name(NID_secp256k1);
    EC_KEY_set_private_key(key, BN_bin2bn(private_key, 32, nullptr));
    EC_KEY_generate_key(key);

    const EC_POINT* pub = EC_KEY_get0_public_key(key);
    const EC_GROUP* group = EC_KEY_get0_group(key);
    unsigned char pub_bytes[65];
    size_t len = EC_POINT_point2oct(group, pub, POINT_CONVERSION_UNCOMPRESSED, pub_bytes, 65, nullptr);
    public_key_hex = "";
    for (size_t i = 0; i < len; i++) {
        char buf[3]; sprintf(buf, "%02x", pub_bytes[i]);
        public_key_hex += buf;
    }

    // Address: SHA-256 + RIPEMD-160 of public key (P2PKH)
    unsigned char hash1[32], hash2[20];
    SHA256(pub_bytes, len, hash1);
    EVP_MD_CTX* md = EVP_MD_CTX_new();
    EVP_DigestInit_ex(md, EVP_ripemd160(), nullptr);
    EVP_DigestUpdate(md, hash1, 32);
    EVP_DigestFinal_ex(md, hash2, nullptr);
    EVP_MD_CTX_free(md);

    std::vector<unsigned char> payload = {0x00};
    payload.insert(payload.end(), hash2, hash2 + 20);
    unsigned char checksum[32];
    SHA256(payload.data(), payload.size(), hash1);
    SHA256(hash1, 32, checksum);
    payload.insert(payload.end(), checksum, checksum + 4);
    address = "1" + base58_encode(payload);

    EC_KEY_free(key);
}

bool KhutiWallet::create_new(const std::string& password, const std::string& save_path) {
    wallet_path = save_path;
    generate_keypair();
    is_loaded = true;

    nlohmann::json vault;
    vault["address"] = address;
    vault["public_key"] = public_key_hex;
    vault["created_at"] = std::time(nullptr);

    std::string json_str = vault.dump();
    std::vector<unsigned char> plaintext(json_str.begin(), json_str.end());

    KhutiEnigma enigma;
    auto encrypted = enigma.encrypt(plaintext, password);

    std::ofstream file(save_path, std::ios::binary);
    file.write((char*)encrypted.data(), encrypted.size());
    file.close();

    std::cout << "🩶 Khuti wallet created at: " << save_path << "\n";
    std::cout << "Address: " << address << "\n";
    return true;
}

bool KhutiWallet::load_from_file(const std::string& password, const std::string& path) {
    wallet_path = path;
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::vector<unsigned char> encrypted((std::istreambuf_iterator<char>(file)), {});
    file.close();

    KhutiEnigma enigma;
    std::vector<unsigned char> plaintext;
    try {
        plaintext = enigma.decrypt(encrypted, password);
    } catch (...) { return false; }

    nlohmann::json vault = nlohmann::json::parse(plaintext);
    address = vault["address"];
    public_key_hex = vault["public_key"];
    is_loaded = true;
    std::cout << "🩶 Khuti wallet loaded: " << address << "\n";
    return true;
}

bool KhutiWallet::save_to_file(const std::string& password) {
    nlohmann::json vault;
    vault["address"] = address;
    vault["public_key"] = public_key_hex;
    vault["balance_onchain"] = onchain_balance;
    vault["hawala_pending"] = hawala_pending_balance;

    std::string json_str = vault.dump();
    std::vector<unsigned char> plaintext(json_str.begin(), json_str.end());

    KhutiEnigma enigma;
    auto encrypted = enigma.encrypt(plaintext, password);

    std::ofstream file(wallet_path, std::ios::binary);
    file.write((char*)encrypted.data(), encrypted.size());
    file.close();
    return true;
}

// -- Hawala --
std::string KhutiWallet::createHawalaToken(const std::string& receiver_addr, double amount, uint64_t nonce) {
    nlohmann::json token;
    token["sender"] = address;
    token["receiver"] = receiver_addr;
    token["amount"] = amount;
    token["nonce"] = nonce;
    token["expiry"] = std::time(nullptr) + 604800; // 7 days

    std::string payload = token.dump();

    // Sign with private key (ECDSA)
    unsigned char hash[32];
    SHA256((const unsigned char*)payload.c_str(), payload.size(), hash);

    EC_KEY* key = EC_KEY_new_by_curve_name(NID_secp256k1);
    BN_bin2bn(private_key, 32, EC_KEY_get0_private_key(key));
    EC_KEY_generate_key(key);

    ECDSA_SIG* sig = ECDSA_do_sign(hash, 32, key);
    unsigned char der[128];
    int der_len = i2d_ECDSA_SIG(sig, nullptr);
    unsigned char* der_ptr = der;
    i2d_ECDSA_SIG(sig, &der_ptr);

    token["signature"] = std::string((char*)der, der_len);
    ECDSA_SIG_free(sig);
    EC_KEY_free(key);

    std::string token_str = token.dump();
    // Base64 encode for easy transfer
    return base58_encode(std::vector<unsigned char>(token_str.begin(), token_str.end()));
}

bool KhutiWallet::acceptHawalaToken(const std::string& base64_token) {
    auto token_bytes = base58_decode(base64_token);
    std::string token_str(token_bytes.begin(), token_bytes.end());
    nlohmann::json token = nlohmann::json::parse(token_str);

    if (token["expiry"].get<uint64_t>() < (uint64_t)std::time(nullptr)) {
        std::cerr << "🩶 Hawala token expired\n";
        return false;
    }

    // Verify signature (simplified: we trust the sender's address)
    // In production, recover public key from signature.
    hawala_pending_balance += token["amount"].get<double>();
    std::cout << "🩶 Hawala accepted! +" << token["amount"] << " BTC pending\n";
    return true;
}

// -- SHA-256 Sweeper (Vintage UTXO hunter) --
void KhutiWallet::sweep_vintage_utxos() {
    std::cout << "🩶 Starting vintage sweeper...\n";
    unsigned char block_header[80] = {0};
    unsigned char target[32] = {0};
    target[31] = 0x01; // Very low difficulty for testing

    uint64_t hashes = 0;
    auto start = std::chrono::steady_clock::now();

    for (uint32_t nonce = 0; nonce < 0xFFFFFFFF; nonce++) {
        memcpy(&block_header[76], &nonce, 4);

        unsigned char hash[32];
        SHA256_CTX ctx;
        SHA256_Init(&ctx);
        SHA256_Update(&ctx, block_header, 80);
        SHA256_Final(hash, &ctx);

        SHA256_Init(&ctx);
        SHA256_Update(&ctx, hash, 32);
        SHA256_Final(hash, &ctx);

        hashes++;

        if (hashes % 1000000 == 0) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - start).count();
            hashrate_khs = (hashes / elapsed) / 1000.0;
            std::cout << "🩶 Hashrate: " << hashrate_khs << " KH/s, nonce: " << nonce << "\n";
        }

        if (memcmp(hash, target, 32) < 0) {
            std::cout << "🩶 🎉 FOUND VINTAGE SHARE! Nonce: " << nonce << "\n";
            // Here you would construct a block and broadcast via NetworkClient
            break;
        }
    }
}

void KhutiWallet::update_balance_from_node(NetworkClient& client) {
    // Placeholder: query blockchain via RPC
    // ...
}
