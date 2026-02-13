#pragma once
#include <string>
#include <vector>

namespace filestore {

bool append_line(const std::string& path, const std::string& line, std::string& err);
std::vector<std::string> read_lines(const std::string& path);

} // namespace filestore
