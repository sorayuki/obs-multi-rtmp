#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <chrono>

std::optional<std::pair<int, int>> ParseResolution(std::string_view res);
std::string FormatBitrate(double bitsPerSecond);
std::string FormatDuration(std::chrono::seconds total);
