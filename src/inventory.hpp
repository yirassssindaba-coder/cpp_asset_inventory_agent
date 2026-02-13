#pragma once
#include <string>
#include "mini_json.hpp"

namespace inventory {

minijson::Value build_asset_payload(const std::string& agent_version);

bool validate_asset_schema(const minijson::Value& root, std::string& why);

std::string make_asset_id(const std::string& hostname);

} // namespace inventory
