#pragma once
#include <string>

namespace logutil {
    void ensure_dirs();
    void info(const std::string& tag, const std::string& msg);
    void warn(const std::string& tag, const std::string& msg);
    void error(const std::string& tag, const std::string& msg);
}
