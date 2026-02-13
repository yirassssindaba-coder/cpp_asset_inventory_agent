#include "logger.hpp"
#include <fstream>
#include <iostream>
#include <ctime>
#include <filesystem>

static std::string now_iso() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

namespace logutil {
    void ensure_dirs() {
        std::filesystem::create_directories("logs");
        std::filesystem::create_directories("data");
    }

    static void write(const std::string& level, const std::string& tag, const std::string& msg) {
        ensure_dirs();
        std::string line = "[" + now_iso() + "][" + level + "][" + tag + "] " + msg;
        std::ofstream f("logs/app.log", std::ios::app);
        f << line << "\n";
        // also print to stderr for visibility
        if (level == "ERROR") std::cerr << line << "\n";
    }

    void info(const std::string& tag, const std::string& msg) { write("INFO", tag, msg); }
    void warn(const std::string& tag, const std::string& msg) { write("WARN", tag, msg); }
    void error(const std::string& tag, const std::string& msg) { write("ERROR", tag, msg); }
}
