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

TEST_CASE("ApplyPresetServer coerces null or non-object serviceParam to an object") {
    nlohmann::json nullVal; // default-constructed json is null
    auto out1 = ApplyPresetServer(nullVal, "rtmp://live.example/app");
    CHECK(out1.is_object());
    CHECK(out1["server"] == "rtmp://live.example/app");

    nlohmann::json arrVal = nlohmann::json::array();
    auto out2 = ApplyPresetServer(arrVal, "rtmp://live.example/app");
    CHECK(out2.is_object());
    CHECK(out2["server"] == "rtmp://live.example/app");
}

TEST_CASE("ApplyPresetServer clears an existing server when serverUrl is empty") {
    nlohmann::json j;
    j["server"] = "rtmp://a.rtmp.youtube.com/live2"; // leftover from a previously selected preset
    j["key"] = "abc";
    auto out = ApplyPresetServer(j, "");
    CHECK(out["server"] == "");
    CHECK(out["key"] == "abc"); // preserved
}
