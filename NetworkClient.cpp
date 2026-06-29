#include "NetworkClient.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <arpa/inet.h>
#include <fcntl.h>

NetworkClient::NetworkClient() : sockfd(-1), connected(false) {}

NetworkClient::~NetworkClient() { disconnect(); }

bool NetworkClient::connect(const std::string& host, int port) {
    if (isConnected()) return false;

    struct addrinfo hints{}, *res;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0)
        return false;

    for (auto* p = res; p; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) continue;

        struct timeval tv{5, 0};
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        if (::connect(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
            connected = true;
            break;
        }
        close(sockfd);
        sockfd = -1;
    }

    freeaddrinfo(res);
    return connected;
}

void NetworkClient::disconnect() {
    if (sockfd != -1) {
        close(sockfd);
        sockfd = -1;
    }
    connected = false;
}

bool NetworkClient::isConnected() const {
    return connected && sockfd != -1;
}

bool NetworkClient::sendMessage(const std::string& message) {
    if (!isConnected()) return false;

    const char* data = message.c_str();
    size_t remaining = message.size();
    ssize_t sent = 0;

    while (remaining > 0) {
        ssize_t n = ::send(sockfd, data + sent, remaining, 0);
        if (n <= 0) {
            disconnect();
            return false;
        }
        remaining -= n;
        sent += n;
    }
    return true;
}

std::string NetworkClient::receiveMessage() {
    if (!isConnected()) return "";

    const size_t BUFSIZE = 4096;
    char buffer[BUFSIZE];
    std::string result;

    while (true) {
        ssize_t n = ::recv(sockfd, buffer, BUFSIZE - 1, 0);
        if (n <= 0) {
            if (result.empty()) disconnect();
            break;
        }
        buffer[n] = '\0';
        result += std::string(buffer, n);
        if (result.find('\n') != std::string::npos) break;
    }
    return result;
}
