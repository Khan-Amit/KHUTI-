#ifndef KHUTIENIGMA_H
#define KHUTIENIGMA_H

#include <string>
#include <vector>

class KhutiEnigma {
private:
    unsigned char rotor1[256], rotor2[256], rotor3[256];
    void initRotors(const std::string& password);

public:
    // Encrypts plaintext -> Enigma-rotor scramble -> AES-256-GCM
    std::vector<unsigned char> encrypt(const std::vector<unsigned char>& plaintext,
                                       const std::string& password);

    // Decrypts: AES-GCM -> inverse rotor scramble -> plaintext
    std::vector<unsigned char> decrypt(const std::vector<unsigned char>& ciphertext,
                                       const std::string& password);
};

#endif
