#pragma once

#include "json.hpp"
#include <string>
#include <optional>

template<class T>
static bool IsJsonFieldTypeMatch(nlohmann::json::iterator it) {
    if (std::is_same_v<T, int>) return it->is_number_integer();
    else if (std::is_same_v<T, bool>) return it->is_boolean();
    else if (std::is_same_v<T, std::string>) return it->is_string();
    else if (std::is_same_v<T, nlohmann::json>) return it->is_object();
    return false;
}

template<class T>
static std::optional<T> GetJsonField(nlohmann::json& json, const char* key) {
    auto it = json.find(key);
    if (it != json.end() && IsJsonFieldTypeMatch<T>(it))
        return it->get<T>();
    return {};
}
