#include "inventory.hpp"
#include "mini_json.hpp"
#include "http_client.hpp"
#include "logger.hpp"
#include <iostream>
#include <thread>
#include <chrono>

static void usage() {
    std::cout << "Asset Inventory Agent (C++)\n"
              << "Usage:\n"
              << "  asset_agent --host 127.0.0.1 --port 8080 --path /api/assets --retries 3 --timeout 2000\n";
}

static std::string arg_val(int& i, int argc, char** argv) {
    if (i + 1 >= argc) return "";
    return std::string(argv[++i]);
}

int main(int argc, char** argv) {
    logutil::ensure_dirs();

    std::string host = "127.0.0.1";
    int port = 8080;
    std::string path = "/api/assets";
    int retries = 3;
    int timeout_ms = 2000;
    std::string agent_version = "1.0.0";

    for (int i=1;i<argc;i++) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { usage(); return 0; }
        else if (a == "--host") host = arg_val(i, argc, argv);
        else if (a == "--port") port = std::atoi(arg_val(i, argc, argv).c_str());
        else if (a == "--path") path = arg_val(i, argc, argv);
        else if (a == "--retries") retries = std::atoi(arg_val(i, argc, argv).c_str());
        else if (a == "--timeout") timeout_ms = std::atoi(arg_val(i, argc, argv).c_str());
        else if (a == "--version") agent_version = arg_val(i, argc, argv);
    }
    if (port <= 0) port = 8080;
    if (retries < 0) retries = 0;
    if (timeout_ms < 200) timeout_ms = 200;

    auto payload = inventory::build_asset_payload(agent_version);
    std::string why;
    if (!inventory::validate_asset_schema(payload, why)) {
        logutil::error("agent", "payload schema invalid: " + why);
        std::cerr << "[ERROR] payload schema invalid: " << why << "\n";
        return 1;
    }

    std::string body = minijson::stringify(payload, true);

    logutil::info("agent", "sending asset payload to http://" + host + ":" + std::to_string(port) + path);

    int attempt = 0;
    httpclient::Response last;
    while (attempt <= retries) {
        last = httpclient::post_json(host, port, path, body, timeout_ms);
        if (last.status >= 200 && last.status < 300) {
            std::cout << "[OK] Sent asset data. HTTP " << last.status << "\n";
            return 0;
        }
        std::string msg = "attempt " + std::to_string(attempt+1) + " failed: ";
        if (!last.error.empty()) msg += last.error;
        else msg += "HTTP " + std::to_string(last.status) + " body=" + last.body;
        logutil::warn("agent", msg);
        std::cerr << "[WARN] " << msg << "\n";

        if (attempt == retries) break;
        int backoff = 1 << attempt; // 1,2,4...
        std::this_thread::sleep_for(std::chrono::seconds(backoff));
        attempt++;
    }

    // Do not crash the "main workflow": exit code 0 but logs warn (as requested)
    std::cout << "[DONE] Agent finished with warnings. Check logs/app.log\n";
    return 0;
}
