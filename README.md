# KHUTI-
Test 
# 🩶 KHUTI™ – Sovereign Hawala Wallet & Vintage Sweeper

**Copyright © 2026 Seliim Ahmed. All rights reserved.**

---

## 🛡️ LEGAL NOTICE

This software is the **exclusive intellectual property** of **Seliim Ahmed**.

- **Read-Only Access:** You are permitted to view and study the source code for reference only.
- **No Use Without Permission:** Any use, compilation, modification, distribution, or integration into other projects is **strictly forbidden** without prior written permission from Seliim Ahmed.
- **Legal Enforcement:** Unauthorized use, alteration, abomination, or misrepresentation of this software will be met with legal proceedings under applicable intellectual property laws.

For licensing, inquiries, or written permission:
📧 **amit.khanna.1082@gmail.com**

---

## 📖 About Khuti™

Khuti™ (Bengali: খুঁটি – a twisted cloth pocket worn at the waist) is a sovereign, offline-first cryptocurrency wallet and vintage UTXO sweeper. It features:

- **Enigma Vault** – AES-256-GCM + rotor-scrambled encryption.
- **Hawala Settlement** – Offline, peer-to-peer value transfer via signed tokens.
- **SHA-256 Sweeper** – Vintage block hunting for lost UTXOs (2009–2013 era).
- **Web Dashboard** – 🩶 Grey-themed real-time interface served locally.

---

## 🏗️ Architecture

| Component | Language | Role |
| :--- | :--- | :--- |
| `NetworkClient` | C++17 | TCP transport (raw socket) |
| `KhutiEnigma` | C++17 | Encryption vault |
| `KhutiWallet` | C++17 | Key management, Hawala, SHA-256 sweeper |
| `KhutiWebServer` | C++17 | Embedded HTTP server |
| `index.html` | HTML/CSS/JS | Front-end dashboard |

---

## 🚀 Building (For Reference Only)

**You are not authorized to build or run this software without explicit written permission from Seliim Ahmed.**

If you have received permission:

```bash
# Dependencies: g++, libssl-dev, nlohmann/json (header-only)
g++ -std=c++17 -O3 -pthread \
    NetworkClient.cpp KhutiEnigma.cpp KhutiWallet.cpp KhutiWebServer.cpp main.cpp \
    -lssl -lcrypto -o khuti
