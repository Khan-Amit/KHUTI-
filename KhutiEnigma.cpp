#include "KhutiEnigma.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <cstring>
#include <stdexcept>

void KhutiEnigma::initRotors(const std::string& password) {
    unsigned char seed[32];
    SHA256((const unsigned char*)password.c_str(), password.size(), seed);

    for (int i = 0; i < 256; i++) {
        rotor1[i] = (seed[i % 32] + i) % 256;
        rotor2[i] = (seed[(i + 13) % 32] + (i * 3)) % 256;
        rotor3[i] = (seed[(i + 29) % 32] + (i * 7)) % 256;
    }
}

std::vector<unsigned char> KhutiEnigma::encrypt(const std::vector<unsigned char>& plaintext,
                                                const std::string& password) {
    initRotors(password);

    // Step 1: Enigma rotor permutation
    std::vector<unsigned char> transformed(plaintext.size());
    for (size_t i = 0; i < plaintext.size(); i++) {
        int p = i % 256;
        transformed[i] = plaintext[i] ^ rotor1[p] ^ rotor2[(p + i) % 256] ^ rotor3[(p + i * 2) % 256];
    }

    // Step 2: AES-256-GCM (OpenSSL EVP)
    unsigned char key[32];
    SHA256((const unsigned char*)password.c_str(), password.size(), key);

    unsigned char nonce[12];
    RAND_bytes(nonce, sizeof(nonce));

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key, nonce);

    std::vector<unsigned char> ciphertext(transformed.size() + 16);
    int len = 0, ciphertext_len = 0;

    EVP_EncryptUpdate(ctx, ciphertext.data(), &len, transformed.data(), transformed.size());
    ciphertext_len += len;
    EVP_EncryptFinal_ex(ctx, ciphertext.data() + ciphertext_len, &len);
    ciphertext_len += len;

    unsigned char tag[16];
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag);
    EVP_CIPHER_CTX_free(ctx);

    // Build: nonce (12) + ciphertext (variable) + tag (16)
    std::vector<unsigned char> result;
    result.insert(result.end(), nonce, nonce + 12);
    result.insert(result.end(), ciphertext.begin(), ciphertext.begin() + ciphertext_len);
    result.insert(result.end(), tag, tag + 16);

    return result;
}

std::vector<unsigned char> KhutiEnigma::decrypt(const std::vector<unsigned char>& ciphertext,
                                                const std::string& password) {
    if (ciphertext.size() < 28) throw std::runtime_error("Invalid ciphertext");

    initRotors(password);

    unsigned char key[32];
    SHA256((const unsigned char*)password.c_str(), password.size(), key);

    const unsigned char* nonce = ciphertext.data();
    const unsigned char* enc_data = ciphertext.data() + 12;
    size_t enc_len = ciphertext.size() - 28;
    const unsigned char* tag = ciphertext.data() + 12 + enc_len;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key, nonce);

    std::vector<unsigned char> decrypted(enc_len);
    int len = 0, decrypted_len = 0;

    EVP_DecryptUpdate(ctx, decrypted.data(), &len, enc_data, enc_len);
    decrypted_len += len;

    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, (void*)tag);
    int ret = EVP_DecryptFinal_ex(ctx, decrypted.data() + decrypted_len, &len);
    EVP_CIPHER_CTX_free(ctx);

    if (ret <= 0) throw std::runtime_error("Decryption failed (wrong password)");

    // Inverse Enigma rotor permutation
    std::vector<unsigned char> plaintext(decrypted.size());
    for (size_t i = 0; i < decrypted.size(); i++) {
        int p = i % 256;
        plaintext[i] = decrypted[i] ^ rotor1[p] ^ rotor2[(p + i) % 256] ^ rotor3[(p + i * 2) % 256];
    }
    return plaintext;
}
