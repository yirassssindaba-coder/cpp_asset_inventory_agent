#include "http_server.hpp"
#include "logger.hpp"
#include <cstdlib>

int main(int argc, char** argv) {
    logutil::ensure_dirs();
    int port = 8080;
    if (argc >= 2) {
        port = std::atoi(argv[1]);
        if (port <= 0) port = 8080;
    }
    return httpserver::run(port);
}
