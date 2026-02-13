#pragma once
#include <string>

namespace httpclient {

struct Response {
    int status = 0;
    std::string body;
    std::string error;
};

Response post_json(const std::string& host, int port, const std::string& path,
                   const std::string& json_body, int timeout_ms);

} // namespace httpclient
