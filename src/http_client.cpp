#include "http_client.hpp"
#include "logger.hpp"
#include <cstring>
#include <sstream>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netdb.h>
  #include <unistd.h>
#endif

static void sock_close(int fd) {
#ifdef _WIN32
    closesocket((SOCKET)fd);
#else
    close(fd);
#endif
}

static bool sock_init(std::string& err) {
#ifdef _WIN32
    WSADATA wsa{};
    int rc = WSAStartup(MAKEWORD(2,2), &wsa);
    if (rc != 0) { err = "WSAStartup gagal"; return false; }
#endif
    (void)err;
    return true;
}

static void sock_cleanup() {
#ifdef _WIN32
    WSACleanup();
#endif
}

static int connect_tcp(const std::string& host, int port, int timeout_ms, std::string& err) {
    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = nullptr;
    std::string port_s = std::to_string(port);
    int rc = getaddrinfo(host.c_str(), port_s.c_str(), &hints, &res);
    if (rc != 0 || !res) { err = "DNS/addrinfo gagal"; return -1; }

    int fd = -1;
    for (auto p=res; p; p=p->ai_next) {
#ifdef _WIN32
        SOCKET s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == INVALID_SOCKET) continue;
        fd = (int)s;
#else
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
#endif

        // timeout (best-effort)
#ifdef _WIN32
        DWORD tv = (DWORD)timeout_ms;
        setsockopt((SOCKET)fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        setsockopt((SOCKET)fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
#else
        struct timeval tv{};
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif

        if (::connect(
#ifdef _WIN32
            (SOCKET)fd,
#else
            fd,
#endif
            p->ai_addr, (int)p->ai_addrlen) == 0) {
            freeaddrinfo(res);
            return fd;
        }

        sock_close(fd);
        fd = -1;
    }

    freeaddrinfo(res);
    err = "connect gagal (timeout/network unreachable)";
    return -1;
}

static bool send_all(int fd, const std::string& data) {
    const char* p = data.c_str();
    size_t left = data.size();
    while (left > 0) {
#ifdef _WIN32
        int n = send((SOCKET)fd, p, (int)left, 0);
#else
        ssize_t n = send(fd, p, left, 0);
#endif
        if (n <= 0) return false;
        p += n;
        left -= (size_t)n;
    }
    return true;
}

static std::string recv_some(int fd) {
    char buf[4096];
#ifdef _WIN32
    int n = recv((SOCKET)fd, buf, (int)sizeof(buf), 0);
#else
    ssize_t n = recv(fd, buf, sizeof(buf), 0);
#endif
    if (n <= 0) return {};
    return std::string(buf, buf + n);
}

namespace httpclient {

Response post_json(const std::string& host, int port, const std::string& path,
                   const std::string& json_body, int timeout_ms) {
    Response r;
    std::string err;
    if (!sock_init(err)) { r.error = err; return r; }

    int fd = connect_tcp(host, port, timeout_ms, err);
    if (fd < 0) { r.error = err; sock_cleanup(); return r; }

    std::ostringstream req;
    req << "POST " << path << " HTTP/1.1\r\n";
    req << "Host: " << host << ":" << port << "\r\n";
    req << "Content-Type: application/json\r\n";
    req << "Connection: close\r\n";
    req << "Content-Length: " << json_body.size() << "\r\n\r\n";
    req << json_body;

    if (!send_all(fd, req.str())) {
        r.error = "send gagal";
        sock_close(fd);
        sock_cleanup();
        return r;
    }

    std::string resp;
    for (;;) {
        std::string chunk = recv_some(fd);
        if (chunk.empty()) break;
        resp += chunk;
        if (resp.size() > 2*1024*1024) break; // guard
    }

    sock_close(fd);
    sock_cleanup();

    // parse status line
    auto pos = resp.find("\r\n");
    std::string status_line = (pos==std::string::npos) ? resp : resp.substr(0,pos);
    int status = 0;
    {
        std::istringstream ss(status_line);
        std::string httpver;
        ss >> httpver >> status;
    }
    r.status = status;

    // body (very simple)
    auto sep = resp.find("\r\n\r\n");
    if (sep != std::string::npos) r.body = resp.substr(sep + 4);
    else r.body = "";

    return r;
}

} // namespace httpclient
