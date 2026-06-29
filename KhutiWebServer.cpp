void KhutiWebServer::handle_client(int client_fd) {
    char buffer[4096] = {0};
    read(client_fd, buffer, sizeof(buffer) - 1);
    std::string request(buffer);
    std::istringstream req(request);
    std::string method, path, version;
    req >> method >> path >> version;

    // Parse body (for POST requests)
    std::string body;
    if (method == "POST") {
        size_t pos = request.find("\r\n\r\n");
        if (pos != std::string::npos) {
            body = request.substr(pos + 4);
        }
    }

    std::string response_body, content_type = "text/html";

    // ---- ROUTES ----
    if (path == "/" || path == "/index.html") {
        response_body = get_embedded_html();
    }
    else if (path == "/status") {
        nlohmann::json j;
        j["address"] = wallet.getAddress();
        j["balance_onchain"] = wallet.getOnChainBalance();
        j["balance_hawala"] = wallet.getHawalaPending();
        j["total"] = j["balance_onchain"].get<double>() + j["balance_hawala"].get<double>();
        j["block_height"] = wallet.getLastBlockHeight();
        j["hashrate"] = wallet.getSweeperHashRate();
        response_body = j.dump();
        content_type = "application/json";
    }
    else if (path == "/sweep" && method == "POST") {
        std::thread([this]() { wallet.sweep_vintage_utxos(); }).detach();
        response_body = R"({"status":"sweep_started"})";
        content_type = "application/json";
    }
    // ---- HAWALA: CREATE TOKEN (SEND) ----
    else if (path == "/hawala/create" && method == "POST") {
        try {
            nlohmann::json req_json = nlohmann::json::parse(body);
            std::string receiver = req_json["receiver"];
            double amount = req_json["amount"];
            uint64_t nonce = (uint64_t)std::time(nullptr);
            std::string token = wallet.createHawalaToken(receiver, amount, nonce);
            nlohmann::json resp;
            resp["status"] = "created";
            resp["token"] = token;
            response_body = resp.dump();
            content_type = "application/json";
        } catch (...) {
            nlohmann::json resp;
            resp["status"] = "error";
            resp["reason"] = "Invalid JSON or parameters";
            response_body = resp.dump();
            content_type = "application/json";
        }
    }
    // ---- HAWALA: VERIFY TOKEN (RECEIVE) ----
    else if (path == "/hawala/verify" && method == "POST") {
        try {
            nlohmann::json req_json = nlohmann::json::parse(body);
            std::string token = req_json["token"];
            bool accepted = wallet.acceptHawalaToken(token);
            nlohmann::json resp;
            if (accepted) {
                resp["status"] = "accepted";
                resp["amount"] = wallet.getHawalaPending(); // returns updated pending
            } else {
                resp["status"] = "rejected";
                resp["reason"] = "Invalid signature or expired";
            }
            response_body = resp.dump();
            content_type = "application/json";
        } catch (...) {
            nlohmann::json resp;
            resp["status"] = "error";
            resp["reason"] = "Invalid JSON";
            response_body = resp.dump();
            content_type = "application/json";
        }
    }
    else {
        response_body = "<h1>🩶 Khuti Rig</h1><p>Endpoint not found</p>";
    }

    // Build and send response
    std::ostringstream resp;
    resp << "HTTP/1.1 200 OK\r\n";
    resp << "Content-Type: " << content_type << "\r\n";
    resp << "Content-Length: " << response_body.size() << "\r\n";
    resp << "Access-Control-Allow-Origin: *\r\n";
    resp << "Connection: close\r\n\r\n";
    resp << response_body;

    send(client_fd, resp.str().c_str(), resp.str().size(), 0);
    close(client_fd);
}
