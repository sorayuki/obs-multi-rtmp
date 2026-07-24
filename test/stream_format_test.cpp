#include "doctest.h"
#include "stream-format.h"

TEST_CASE("ParseResolution parses WxH with spaces") {
    auto r = ParseResolution("1920x1080");
    REQUIRE(r.has_value());
    CHECK(r->first == 1920);
    CHECK(r->second == 1080);

    auto r2 = ParseResolution("  1280 x 720 ");
    REQUIRE(r2.has_value());
    CHECK(r2->first == 1280);
    CHECK(r2->second == 720);
}

TEST_CASE("ParseResolution rejects garbage") {
    CHECK_FALSE(ParseResolution("abc").has_value());
    CHECK_FALSE(ParseResolution("1920").has_value());
    CHECK_FALSE(ParseResolution("").has_value());
}

TEST_CASE("FormatBitrate scales units") {
    CHECK(FormatBitrate(0) == "0 bps");
    CHECK(FormatBitrate(500).find("bps") != std::string::npos);
    CHECK(FormatBitrate(2'000'000).find("Mbps") != std::string::npos);
}

TEST_CASE("FormatDuration is HH:MM:SS") {
    CHECK(FormatDuration(std::chrono::seconds(0)) == "00:00:00");
    CHECK(FormatDuration(std::chrono::seconds(3661)) == "01:01:01");
}
