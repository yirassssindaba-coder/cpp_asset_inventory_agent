#include "file_store.hpp"
#include <fstream>
#include <filesystem>

namespace filestore {

bool append_line(const std::string& path, const std::string& line, std::string& err) {
    try { std::filesystem::create_directories(std::filesystem::path(path).parent_path()); } catch (...) {}
    std::ofstream f(path, std::ios::app);
    if (!f) { err = "tidak bisa membuka file store"; return false; }
    f << line << "\n";
    return true;
}

std::vector<std::string> read_lines(const std::string& path) {
    std::vector<std::string> out;
    std::ifstream f(path);
    if (!f) return out;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty()) out.push_back(line);
    }
    return out;
}

} // namespace filestore
