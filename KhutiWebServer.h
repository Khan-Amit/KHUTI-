#ifndef KHUTIWEBSERVER_H
#define KHUTIWEBSERVER_H

#include "KhutiWallet.h"

class KhutiWebServer {
private:
    KhutiWallet& wallet;
    int server_fd;

    std::string build_response(const std::string& body, const std::string& content_type = "text/html");
    std::string get_embedded_html();
    void handle_client(int client_fd);

public:
    KhutiWebServer(KhutiWallet& w);
    void start(int port = 8080);
    void stop();
};

#endif
