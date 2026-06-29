cmake_minimum_required(VERSION 3.10)
project(KhutiRig)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenSSL (for SHA-256, ECDSA, AES-GCM)
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

add_executable(khuti
    NetworkClient.cpp
    KhutiEnigma.cpp
    KhutiWallet.cpp
    KhutiWebServer.cpp
    main.cpp
)

target_link_libraries(khuti
    ${OPENSSL_LIBRARIES}
    pthread
)
