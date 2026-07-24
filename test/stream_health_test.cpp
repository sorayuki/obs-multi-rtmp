#include "doctest.h"
#include "stream-health.h"

TEST_CASE("health thresholds") {
    CHECK(HealthFromStats(0, 1000, 0.0f) == StreamHealth::Good);
    CHECK(HealthFromStats(20, 1000, 0.1f) == StreamHealth::Warn);   // 2% dropped
    CHECK(HealthFromStats(100, 1000, 0.1f) == StreamHealth::Bad);   // 10% dropped
    CHECK(HealthFromStats(0, 1000, 0.8f) == StreamHealth::Bad);     // congested
    CHECK(HealthFromStats(0, 0, 0.0f) == StreamHealth::Good);       // no frames yet
}

TEST_CASE("health thresholds boundary-exact") {
    CHECK(HealthFromStats(10, 1000, 0.0f) == StreamHealth::Warn);   // pct == 1.0% exactly
    CHECK(HealthFromStats(50, 1000, 0.0f) == StreamHealth::Warn);   // pct == 5.0% exactly (NOT Bad)
    CHECK(HealthFromStats(51, 1000, 0.0f) == StreamHealth::Bad);    // pct just over 5%
    CHECK(HealthFromStats(0, 1000, 0.3f) == StreamHealth::Warn);    // congestion == 0.3 exactly
    CHECK(HealthFromStats(0, 1000, 0.7f) == StreamHealth::Bad);     // congestion == 0.7 exactly
}
