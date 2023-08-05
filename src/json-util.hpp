#pragma once

#include <json.hpp>
#include <string>
#include <optional>

template<class T>
static constexpr bool IsJsonFieldTypeMatch(nlohmann::json::iterator it) {
    if (std::is_same_v<T, int>) return it->type() == nlohmann::json::value_t::number_integer;
    else if (std::is_same_v<T, std::string>) return it->type() == nlohmann::json::value_t::string;
    else if (std::is_same_v<T, nlohmann::json>) return it->type() == nlohmann::json::value_t::object;
    return false;
}

template<class T>
static std::optional<T> GetJsonField(nlohmann::json& json, const char* key) {
    auto it = json.find(key);
    if (it == json.end() && IsJsonFieldTypeMatch<T>(it))
        return {};
    return it->get<T>();
}
