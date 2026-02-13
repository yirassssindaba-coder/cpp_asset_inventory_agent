#include "http_server.hpp"
#include "mini_json.hpp"
#include "file_store.hpp"
#include "logger.hpp"
#include "inventory.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
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

static std::string read_request(int fd) {
    std::string data;
    char buf[4096];
    for (;;) {
#ifdef _WIN32
        int n = recv((SOCKET)fd, buf, (int)sizeof(buf), 0);
#else
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
#endif
        if (n <= 0) break;
        data.append(buf, buf + n);
        if (data.size() > 4*1024*1024) break;
        // crude: stop if connection closes
        if (data.find("\r\n\r\n") != std::string::npos) {
            // may still need body; handled later by content-length
        }
    }
    return data;
}

static bool parse_start_line(const std::string& req, std::string& method, std::string& path) {
    auto end = req.find("\r\n");
    if (end == std::string::npos) return false;
    std::istringstream ss(req.substr(0, end));
    ss >> method >> path;
    return !method.empty() && !path.empty();
}

static std::string get_header(const std::string& req, const std::string& key) {
    std::string k = key;
    std::transform(k.begin(), k.end(), k.begin(), [](unsigned char c){ return (char)std::tolower(c); });

    auto head_end = req.find("\r\n\r\n");
    if (head_end == std::string::npos) return "";
    std::string head = req.substr(0, head_end);

    std::istringstream ss(head);
    std::string line;
    std::getline(ss, line); // start line
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back()=='\r') line.pop_back();
        auto pos = line.find(':');
        if (pos == std::string::npos) continue;
        std::string hk = line.substr(0, pos);
        std::string hv = line.substr(pos + 1);
        while (!hv.empty() && hv.front()==' ') hv.erase(hv.begin());
        std::transform(hk.begin(), hk.end(), hk.begin(), [](unsigned char c){ return (char)std::tolower(c); });
        if (hk == k) return hv;
    }
    return "";
}

static std::string get_body(const std::string& req) {
    auto sep = req.find("\r\n\r\n");
    if (sep == std::string::npos) return "";
    return req.substr(sep + 4);
}

static std::string html_dashboard() {
    return R"(<!doctype html>
<html>
<head>
  <meta charset="utf-8"/>
  <title>Asset Dashboard</title>
  <style>
    body{font-family:system-ui,Segoe UI,Arial; margin:24px;}
    h1{margin:0 0 12px 0;}
    .meta{color:#555; margin-bottom:16px;}
    table{border-collapse:collapse; width:100%;}
    th,td{border:1px solid #ddd; padding:10px; text-align:left; font-size:14px;}
    th{background:#f6f6f6;}
    .pill{display:inline-block; padding:2px 8px; border-radius:999px; background:#e7f7ef; color:#137a3a; font-size:12px;}
    code{background:#f5f5f5; padding:2px 6px; border-radius:6px;}
  </style>
</head>
<body>
  <h1>Asset Inventory Dashboard <span class="pill">local</span></h1>
  <div class="meta">Endpoint: <code>/api/assets</code> • Export: <code>/export.csv</code></div>
  <table>
    <thead>
      <tr>
        <th>Asset ID</th>
        <th>Hostname</th>
        <th>OS</th>
        <th>CPU</th>
        <th>RAM (MB)</th>
        <th>Disk (total/free GB)</th>
        <th>Last Seen (UTC)</th>
      </tr>
    </thead>
    <tbody id="rows"></tbody>
  </table>

<script>
async function load(){
  const r = await fetch('/api/assets');
  const arr = await r.json();
  const tbody = document.getElementById('rows');
  tbody.innerHTML = '';
  for (const a of arr){
    const disks = (a.disks||[]).map(d => `${d.mount}:${d.total_gb}/${d.free_gb}`).join(' | ');
    const tr = document.createElement('tr');
    tr.innerHTML = `
      <td>${a.asset_id||''}</td>
      <td>${a.hostname||''}</td>
      <td>${a.os||''}</td>
      <td>${a.cpu_model||''} (${a.cpu_cores||''})</td>
      <td>${a.ram_total_mb||''}</td>
      <td>${disks}</td>
      <td>${a.timestamp_utc||''}</td>`;
    tbody.appendChild(tr);
  }
}
load();
</script>
</body>
</html>)";
}

static std::string http_response(int status, const std::string& content_type, const std::string& body) {
    std::ostringstream o;
    o << "HTTP/1.1 " << status << " ";
    if (status==200) o << "OK";
    else if (status==201) o << "Created";
    else if (status==400) o << "Bad Request";
    else if (status==404) o << "Not Found";
    else o << "Error";
    o << "\r\n";
    o << "Content-Type: " << content_type << "\r\n";
    o << "Connection: close\r\n";
    o << "Content-Length: " << body.size() << "\r\n\r\n";
    o << body;
    return o.str();
}

static std::string json_array_from_store() {
    auto lines = filestore::read_lines("data/assets.jsonl");
    std::vector<minijson::Value> items;
    for (const auto& ln : lines) {
        try {
            items.push_back(minijson::parse(ln));
        } catch (...) {}
    }
    minijson::Value root = minijson::Value::array(std::move(items));
    return minijson::stringify(root, true);
}

static std::string csv_from_store() {
    auto lines = filestore::read_lines("data/assets.jsonl");
    std::ostringstream o;
    o << "asset_id,hostname,os,cpu_model,cpu_cores,ram_total_mb,timestamp_utc,disks\n";
    for (const auto& ln : lines) {
        try {
            auto v = minijson::parse(ln);
            std::string disks;
            if (v.has("disks") && v.at("disks").is_array()) {
                for (size_t i=0;i<v.at("disks").a.size(); ++i) {
                    const auto& d = v.at("disks").a[i];
                    std::string part;
                    if (d.is_object()) {
                        part = d.at("mount").s + ":" + std::to_string((long long)d.at("total_gb").num) + "/" + std::to_string((long long)d.at("free_gb").num);
                    }
                    disks += part;
                    if (i+1 < v.at("disks").a.size()) disks += " | ";
                }
            }
            auto esc = [](std::string s){
                // naive CSV escape
                bool need = s.find(',')!=std::string::npos || s.find('"')!=std::string::npos || s.find('\n')!=std::string::npos;
                if (!need) return s;
                std::string out="\"";
                for (char c: s) { if (c=='"') out += "\"\""; else out += c; }
                out += "\"";
                return out;
            };
            o << esc(v.at("asset_id").s) << ","
              << esc(v.at("hostname").s) << ","
              << esc(v.at("os").s) << ","
              << esc(v.at("cpu_model").s) << ","
              << (long long)v.at("cpu_cores").num << ","
              << (long long)v.at("ram_total_mb").num << ","
              << esc(v.at("timestamp_utc").s) << ","
              << esc(disks) << "\n";
        } catch (...) {}
    }
    return o.str();
}

namespace httpserver {

int run(int port) {
    std::string err;
    if (!sock_init(err)) {
        logutil::error("server", err);
        return 1;
    }

    int srv = (int)socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
    if (srv == (int)INVALID_SOCKET) { logutil::error("server", "socket() gagal"); sock_cleanup(); return 1; }
#else
    if (srv < 0) { logutil::error("server", "socket() gagal"); sock_cleanup(); return 1; }
#endif

    int opt = 1;
#ifdef _WIN32
    setsockopt((SOCKET)srv, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(
#ifdef _WIN32
        (SOCKET)srv,
#else
        srv,
#endif
        (sockaddr*)&addr, sizeof(addr)) != 0) {
        logutil::error("server", "bind() gagal (port mungkin dipakai)");
        sock_close(srv);
        sock_cleanup();
        return 1;
    }

    if (listen(
#ifdef _WIN32
        (SOCKET)srv,
#else
        srv,
#endif
        16) != 0) {
        logutil::error("server", "listen() gagal");
        sock_close(srv);
        sock_cleanup();
        return 1;
    }

    logutil::info("server", "running on http://localhost:" + std::to_string(port));

    while (true) {
        sockaddr_in caddr{};
#ifdef _WIN32
        int clen = sizeof(caddr);
        SOCKET c = accept((SOCKET)srv, (sockaddr*)&caddr, &clen);
        if (c == INVALID_SOCKET) continue;
        int fd = (int)c;
#else
        socklen_t clen = sizeof(caddr);
        int fd = accept(srv, (sockaddr*)&caddr, &clen);
        if (fd < 0) continue;
#endif

        std::string req = read_request(fd);
        std::string method, path;
        if (!parse_start_line(req, method, path)) {
            send_all(fd, http_response(400, "text/plain", "bad request"));
            sock_close(fd);
            continue;
        }

        // Ensure body length if content-length exists (our read_request may have already read all; ok for small payloads)
        std::string body = get_body(req);

        if (method == "GET" && path == "/") {
            auto html = html_dashboard();
            send_all(fd, http_response(200, "text/html; charset=utf-8", html));
        } else if (method == "GET" && path == "/api/assets") {
            auto js = json_array_from_store();
            send_all(fd, http_response(200, "application/json; charset=utf-8", js));
        } else if (method == "GET" && path == "/export.csv") {
            auto csv = csv_from_store();
            send_all(fd, http_response(200, "text/csv; charset=utf-8", csv));
        } else if (method == "POST" && path == "/api/assets") {
            try {
                auto v = minijson::parse(body);
                std::string why;
                if (!inventory::validate_asset_schema(v, why)) {
                    send_all(fd, http_response(400, "application/json; charset=utf-8",
                        std::string("{\"ok\":false,\"error\":\"schema_invalid\",\"detail\":\"") + why + "\"}"));
                } else {
                    std::string line = minijson::stringify(v, false);
                    std::string ferr;
                    if (!filestore::append_line("data/assets.jsonl", line, ferr)) {
                        send_all(fd, http_response(500, "application/json; charset=utf-8",
                            std::string("{\"ok\":false,\"error\":\"store_failed\"}")));
                    } else {
                        send_all(fd, http_response(201, "application/json; charset=utf-8",
                            std::string("{\"ok\":true}")));
                    }
                }
            } catch (const std::exception& e) {
                send_all(fd, http_response(400, "application/json; charset=utf-8",
                    std::string("{\"ok\":false,\"error\":\"invalid_json\",\"detail\":\"") + e.what() + "\"}"));
            }
        } else {
            send_all(fd, http_response(404, "text/plain", "not found"));
        }

        sock_close(fd);
    }

    // never reached
    sock_close(srv);
    sock_cleanup();
    return 0;
}

} // namespace httpserver
