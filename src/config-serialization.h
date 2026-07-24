#pragma once
#include <string>
#include <string_view>
#include "output-config.h"

std::string ConfigToJsonString(const MultiOutputConfig& config);
MultiOutputConfig ConfigFromJsonString(std::string_view json, std::string* errorOut = nullptr);
void RemapImportedIds(MultiOutputConfig& imported, const MultiOutputConfig& existing);
