#include "platform.hpp"
#include <thread>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <fstream>
#include <cstring>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <Lmcons.h>
#else
  #include <unistd.h>
  #include <sys/utsname.h>
  #include <sys/sysinfo.h>
#endif

namespace platforminfo {

std::string hostname() {
#ifdef _WIN32
    char buf[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD sz = MAX_COMPUTERNAME_LENGTH + 1;
    if (GetComputerNameA(buf, &sz)) return std::string(buf);
    return "unknown";
#else
    char buf[256];
    if (gethostname(buf, sizeof(buf)) == 0) return std::string(buf);
    return "unknown";
#endif
}

std::string os_name() {
#ifdef _WIN32
    // Best-effort: report Windows + build via GetVersionEx (may be generic on newer Windows)
    OSVERSIONINFOEXA osvi{};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    if (GetVersionExA((OSVERSIONINFOA*)&osvi)) {
        std::ostringstream o;
        o << "Windows " << osvi.dwMajorVersion << "." << osvi.dwMinorVersion << " (build " << osvi.dwBuildNumber << ")";
        return o.str();
    }
    return "Windows";
#else
    // Prefer /etc/os-release PRETTY_NAME
    std::ifstream f("/etc/os-release");
    if (f) {
        std::string line;
        while (std::getline(f, line)) {
            if (line.rfind("PRETTY_NAME=", 0) == 0) {
                std::string v = line.substr(std::string("PRETTY_NAME=").size());
                if (!v.empty() && v.front()=='"') v.erase(0,1);
                if (!v.empty() && v.back()=='"') v.pop_back();
                return v;
            }
        }
    }
    struct utsname u{};
    if (uname(&u) == 0) {
        return std::string(u.sysname) + " " + u.release;
    }
    return "unknown";
#endif
}

static std::string cpu_brand_x86() {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
    unsigned int regs[4]{0,0,0,0};
    auto cpuid = [&](unsigned int leaf, unsigned int subleaf) {
#ifdef _WIN32
        int r[4];
        __cpuidex(r, (int)leaf, (int)subleaf);
        regs[0]= (unsigned int)r[0]; regs[1]=(unsigned int)r[1]; regs[2]=(unsigned int)r[2]; regs[3]=(unsigned int)r[3];
#else
        unsigned int a,b,c,d;
        __asm__ __volatile__("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(leaf),"c"(subleaf));
        regs[0]=a; regs[1]=b; regs[2]=c; regs[3]=d;
#endif
    };

    char brand[49]; brand[48]=0;
    cpuid(0x80000000,0);
    if (regs[0] < 0x80000004) return "unknown";
    for (unsigned int i=0;i<3;i++){
        cpuid(0x80000002 + i,0);
        std::memcpy(brand + i*16, regs, 16);
    }
    std::string s(brand);
    // trim
    while (!s.empty() && (s.back()==' ' || s.back()=='\0')) s.pop_back();
    while (!s.empty() && s.front()==' ') s.erase(s.begin());
    return s.empty() ? "unknown" : s;
#else
    return "unknown";
#endif
}

std::string cpu_brand() {
#ifndef _WIN32
    // Try /proc/cpuinfo first
    std::ifstream f("/proc/cpuinfo");
    if (f) {
        std::string line;
        while (std::getline(f, line)) {
            if (line.rfind("model name", 0) == 0) {
                auto pos = line.find(':');
                if (pos != std::string::npos) {
                    std::string v = line.substr(pos + 1);
                    while (!v.empty() && v.front()==' ') v.erase(v.begin());
                    return v;
                }
            }
        }
    }
#endif
    return cpu_brand_x86();
}

int cpu_cores() {
    unsigned int c = std::thread::hardware_concurrency();
    return c ? (int)c : 1;
}

long long ram_total_mb() {
#ifdef _WIN32
    MEMORYSTATUSEX st{};
    st.dwLength = sizeof(st);
    if (GlobalMemoryStatusEx(&st)) {
        return (long long)(st.ullTotalPhys / (1024ULL*1024ULL));
    }
    return -1;
#else
    struct sysinfo si{};
    if (sysinfo(&si) == 0) {
        long long total = (long long)si.totalram * (long long)si.mem_unit;
        return total / (1024LL*1024LL);
    }
    return -1;
#endif
}

std::vector<DiskInfo> disks() {
    std::vector<DiskInfo> out;
#ifdef _WIN32
    DWORD drives = GetLogicalDrives();
    for (char letter='A'; letter<='Z'; ++letter) {
        if (drives & (1u << (letter - 'A'))) {
            std::string root;
            root += letter; root += ":\\";
            ULARGE_INTEGER freeBytesAvail{}, totalBytes{}, totalFree{};
            if (GetDiskFreeSpaceExA(root.c_str(), &freeBytesAvail, &totalBytes, &totalFree)) {
                DiskInfo d;
                d.mount = root;
                d.total_gb = (long long)(totalBytes.QuadPart / (1024ULL*1024ULL*1024ULL));
                d.free_gb  = (long long)(totalFree.QuadPart  / (1024ULL*1024ULL*1024ULL));
                out.push_back(d);
            }
        }
    }
#else
    // best-effort: root only
    std::error_code ec;
    auto sp = std::filesystem::space("/", ec);
    if (!ec) {
        DiskInfo d;
        d.mount = "/";
        d.total_gb = (long long)(sp.capacity / (1024ULL*1024ULL*1024ULL));
        d.free_gb  = (long long)(sp.available / (1024ULL*1024ULL*1024ULL));
        out.push_back(d);
    }
#endif
    if (out.empty()) {
        out.push_back({"unknown", -1, -1});
    }
    return out;
}

std::string now_iso_utc() {
    using namespace std::chrono;
    auto now = system_clock::now();
    std::time_t t = system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

} // namespace platforminfo
