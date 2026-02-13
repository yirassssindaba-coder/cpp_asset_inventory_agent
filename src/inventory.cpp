#include "inventory.hpp"
#include "platform.hpp"
#include <sstream>
#include <functional>

namespace inventory {

std::string make_asset_id(const std::string& hostname) {
    // Stable-ish ID: hash(hostname)
    std::hash<std::string> h;
    size_t v = h(hostname);
    std::ostringstream o;
    o << "asset-" << std::hex << v;
    return o.str();
}

minijson::Value build_asset_payload(const std::string& agent_version) {
    using minijson::Value;

    std::string host = platforminfo::hostname();
    std::string os = platforminfo::os_name();
    std::string cpu = platforminfo::cpu_brand();
    int cores = platforminfo::cpu_cores();
    long long ram = platforminfo::ram_total_mb();
    auto disks = platforminfo::disks();

    std::vector<Value> disk_arr;
    for (const auto& d : disks) {
        disk_arr.push_back(Value::object({
            {"mount", Value::string(d.mount)},
            {"total_gb", Value::number((double)d.total_gb)},
            {"free_gb", Value::number((double)d.free_gb)}
        }));
    }

    Value root = Value::object({
        {"asset_id", Value::string(make_asset_id(host))},
        {"hostname", Value::string(host)},
        {"os", Value::string(os)},
        {"cpu_model", Value::string(cpu)},
        {"cpu_cores", Value::number((double)cores)},
        {"ram_total_mb", Value::number((double)ram)},
        {"disks", Value::array(std::move(disk_arr))},
        {"timestamp_utc", Value::string(platforminfo::now_iso_utc())},
        {"agent_version", Value::string(agent_version)}
    });

    return root;
}

static bool is_num_intish(const minijson::Value& v) {
    return v.is_number();
}

bool validate_asset_schema(const minijson::Value& root, std::string& why) {
    using Type = minijson::Value::Type;

    if (!root.is_object()) { why = "root bukan object"; return false; }

    const char* req_str[] = {"asset_id","hostname","os","cpu_model","timestamp_utc","agent_version"};
    for (auto k : req_str) {
        if (!root.has(k) || !root.at(k).is_string()) { why = std::string("field string wajib: ") + k; return false; }
    }

    if (!root.has("cpu_cores") || !is_num_intish(root.at("cpu_cores"))) { why = "field number wajib: cpu_cores"; return false; }
    if (!root.has("ram_total_mb") || !root.at("ram_total_mb").is_number()) { why = "field number wajib: ram_total_mb"; return false; }

    if (!root.has("disks") || !root.at("disks").is_array()) { why = "field array wajib: disks"; return false; }
    for (const auto& d : root.at("disks").a) {
        if (!d.is_object()) { why = "disk item bukan object"; return false; }
        if (!d.has("mount") || !d.at("mount").is_string()) { why = "disk.mount wajib string"; return false; }
        if (!d.has("total_gb") || !d.at("total_gb").is_number()) { why = "disk.total_gb wajib number"; return false; }
        if (!d.has("free_gb") || !d.at("free_gb").is_number()) { why = "disk.free_gb wajib number"; return false; }
    }

    why.clear();
    return true;
}

} // namespace inventory
