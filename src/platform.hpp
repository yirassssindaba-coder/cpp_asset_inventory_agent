#pragma once
#include <string>
#include <vector>

namespace platforminfo {

std::string hostname();
std::string os_name();
std::string cpu_brand();
int cpu_cores();
long long ram_total_mb();

struct DiskInfo {
    std::string mount;
    long long total_gb;
    long long free_gb;
};

std::vector<DiskInfo> disks();

std::string now_iso_utc();

} // namespace platforminfo
