#include "stream-format.h"
#include <regex>
#include <cmath>
#include <cstdio>

std::optional<std::pair<int, int>> ParseResolution(std::string_view res) {
    static const std::regex pattern(R"__(\s*(\d{1,5})\s*x\s*(\d{1,5})\s*)__");
    std::match_results<std::string_view::const_iterator> m;
    if (std::regex_match(res.begin(), res.end(), m, pattern)) {
        return std::make_pair(std::stoi(m[1].str()), std::stoi(m[2].str()));
    }
    return std::nullopt;
}

std::string FormatBitrate(double bps) {
    static const char* units[] = {
        "bps", "Kbps", "Mbps", "Gbps", "Tbps", "Pbps", "Ebps", "Zbps", "Ybps"
    };
    if (bps <= 0)
        return "0 bps";
    const int unitMax = sizeof(units) / sizeof(*units);
    int unitIndex = static_cast<int>(std::log10(bps) / 3);
    if (unitIndex >= unitMax)
        unitIndex = unitMax - 1;
    if (unitIndex < 0)
        unitIndex = 0;
    auto strVal = std::to_string(bps / std::pow(1000, unitIndex)).substr(0, 4);
    if (!strVal.empty() && strVal.back() == '.')
        strVal.pop_back();
    return strVal + " " + units[unitIndex];
}

std::string FormatDuration(std::chrono::seconds total) {
    using namespace std::chrono;
    auto hh = duration_cast<hours>(total);
    auto mm = duration_cast<minutes>(total - hh);
    auto ss = duration_cast<seconds>(total - hh - mm);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                  (int)hh.count(), (int)mm.count(), (int)ss.count());
    return buf;
}
