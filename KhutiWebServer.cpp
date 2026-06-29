#include "KhutiWebServer.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sstream>
#include <thread>
#include <iostream>
#include <nlohmann/json.hpp>

KhutiWebServer::KhutiWebServer(KhutiWallet& w) : wallet(w), server_fd(-1) {}

std::string KhutiWebServer::build_response(const std::string& body, const std::string& content_type) {
    std::ostringstream resp;
    resp << "HTTP/1.1 200 OK\r\n";
    resp << "Content-Type: " << content_type << "\r\n";
    resp << "Content-Length: " << body.size() << "\r\n";
    resp << "Access-Control-Allow-Origin: *\r\n";
    resp << "Connection: close\r\n\r\n";
    resp << body;
    return resp.str();
}

std::string KhutiWebServer::get_embedded_html() {
    return R"(
<!DOCTYPE html>
<html>
<head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>🩶 Khuti Rig</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#1a1a1a;color:#b0b0b0;font-family:'Courier New',monospace;display:flex;justify-content:center;align-items:center;min-height:100vh}
.khuti-panel{background:#2a2a2a;border:1px solid #444;border-radius:16px;padding:30px 40px;max-width:600px;width:100%;box-shadow:0 0 30px rgba(0,0,0,0.8)}
.khuti-header{font-size:28px;color:#c0c0c0;border-bottom:2px solid #444;padding-bottom:10px;margin-bottom:20px;display:flex;justify-content:space-between}
.khuti-header span{color:#888;font-size:18px}
.khuti-row{display:flex;justify-content:space-between;padding:12px 0;border-bottom:1px dotted #333}
.khuti-label{color:#777;text-transform:uppercase;font-size:14px}
.khuti-value{font-weight:bold;color:#e0e0e0;font-size:20px}
.khuti-hawala{color:#a0c0a0}
.khuti-btc{color:#f0b90b}
.khuti-watch{margin-top:20px;padding:15px;background:#111;border-radius:8px;text-align:center;font-size:22px;color:#6f6;letter-spacing:4px;border:1px solid #333}
.khuti-btn{background:#333;border:1px solid #555;color:#ccc;padding:12px 20px;font-family:'Courier New',monospace;font-size:16px;border-radius:8px;cursor:pointer;width:100%;margin-top:15px;transition:0.2s}
.khuti-btn:hover{background:#444;border-color:#888}
.khuti-btn:active{background:#555}
#hashrate{color:#88ddff}
</style>
</head>
<body>
<div class="khuti-panel">
<div class="khuti-header">🩶 KHUTI <span>v2.0</span></div>
<div class="khuti-row"><span class="khuti-label">Address</span><span class="khuti-value" id="address" style="font-size:14px;">1Khuti...</span></div>
<div class="khuti-row"><span class="khuti-label">On-Chain</span><span class="khuti-value khuti-btc" id="onchain">0.00000000 BTC</span></div>
<div class="khuti-row"><span class="khuti-label">Hawala (Pending)</span><span class="khuti-value khuti-hawala" id="hawala">0.00000000 BTC</span></div>
<div class="khuti-row" style="border-bottom:2px solid #555;padding-bottom:15px;"><span class="khuti-label">Total</span><span class="khuti-value" id="total" style="color:#fff;">0.00000000 BTC</span></div>
<div class="khuti-row"><span class="khuti-label">Block Height</span><span class="khuti-value" id="block">876,543</span></div>
<div class="khuti-row"><span class="khuti-label">Sweeper Speed</span><span class="khuti-value" id="hashrate">0 KH/s</span></div>
<div class="khuti-watch" id="clock">⏱️ 00:00:00 UTC</div>
<button class="khuti-btn" onclick="startSweep()">⚡ Start Sweeping UTXOs</button>
<button class="khuti-btn" style="margin-top:5px;" onclick="refreshData()">🔄 Refresh</button>
</div>
<script>
function updateClock(){const n=new Date();document.getElementById('clock').innerText='⏱️ '+n.toUTCString().split(' ')[4]+' UTC'}setInterval(updateClock,1000);updateClock();
async function refreshData(){try{const r=await fetch('/status'),d=await r.json();document.getElementById('address').innerText=d.address.substring(0,16)+'...';document.getElementById('onchain').innerText=d.balance_onchain.toFixed(8)+' BTC';document.getElementById('hawala').innerText=d.balance_hawala.toFixed(8)+' BTC';document.getElementById('total').innerText=d.total.toFixed(8)+' BTC';document.getElementById('block').innerText=d.block_height;document.getElementById('hashrate').innerText=d.hashrate.toFixed(2)+' KH/s'}catch(e){}}setInterval(refreshData,2000);refreshData();
async function startSweep(){document.getElementById('clock').innerText='⚡ SWEEPING...';try{await fetch('/sweep',{method:'POST'});document.getElementById('clock').innerText='✅ Sweep started!'}catch(e){document.getElementById('clock').innerText='❌ Sweep failed'}setTimeout(updateClock,3000)}
</script>
</body></html>
)";
}

void KhutiWebServer::handle_client(int client_fd) {
    char buffer[4096] = {0};
    read(client_fd, buffer, sizeof(buffer) - 1);
    std::string request(buffer);
    std::istringstream req(request);
    std::string method, path, version;
    req >> method >> path >> version;

    std::string body, ctype = "text/html";

    if (path == "/" || path == "/index.html") {
        body = get_embedded_html();
    } else if (path == "/status") {
        nlohmann::json j;
        j["address"] = wallet.getAddress();
        j["balance_onchain"] = wallet.getOnChainBalance();
        j["balance_hawala"] = wallet.getHawalaPending();
        j["total"] = j["balance_onchain"].get<double>() + j["balance_hawala"].get<double>();
        j["block_height"] = wallet.getLastBlockHeight();
        j["hashrate"] = wallet.getSweeperHashRate();
        body = j.dump();
        ctype = "application/json";
    } else if (path == "/sweep" && method == "POST") {
        std::thread([this]() { wallet.sweep_vintage_utxos(); }).detach();
        body = R"({"status":"sweep_started"})";
        ctype = "application/json";
    } else {
        body = "<h1>🩶 Khuti Rig</h1><p>Not found</p>";
    }

    std::string resp = build_response(body, ctype);
    send(client_fd, resp.c_str(), resp.size(), 0);
    close(client_fd);
}

void KhutiWebServer::start(int port) {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);

    std::cout << "🩶 Khuti Web Dashboard: http://localhost:" << port << "\n";

    while (true) {
        int client = accept(server_fd, nullptr, nullptr);
        if (client >= 0) {
            std::thread([this, client]() { handle_client(client); }).detach();
        }
    }
}

void KhutiWebServer::stop() {
    if (server_fd != -1) close(server_fd);
}
