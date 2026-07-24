#include "doctest.h"
#include "platform-presets.h"

TEST_CASE("presets are non-empty and include Custom") {
    auto& p = GetPlatformPresets();
    CHECK(p.size() >= 5);
    bool hasCustom = false;
    for (auto& x : p) if (x.name == "Custom") hasCustom = true;
    CHECK(hasCustom);
}

TEST_CASE("ApplyPresetServer sets the server field") {
    nlohmann::json j;
    j["key"] = "abc";
    auto out = ApplyPresetServer(j, "rtmp://live.example/app");
    CHECK(out["server"] == "rtmp://live.example/app");
    CHECK(out["key"] == "abc"); // preserved
}
