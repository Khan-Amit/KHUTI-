#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <string>

class NetworkClient {
public:
    NetworkClient();
    ~NetworkClient();

    bool connect(const std::string& host, int port);
    void disconnect();
    bool isConnected() const;

    bool sendMessage(const std::string& message);
    std::string receiveMessage();  // Reads until '\n'

private:
    int sockfd;
    bool connected;
};

#endif
