#include "doctest.h"
#include "stream-health.h"

TEST_CASE("health thresholds") {
    CHECK(HealthFromStats(0, 1000, 0.0f) == StreamHealth::Good);
    CHECK(HealthFromStats(20, 1000, 0.1f) == StreamHealth::Warn);   // 2% dropped
    CHECK(HealthFromStats(100, 1000, 0.1f) == StreamHealth::Bad);   // 10% dropped
    CHECK(HealthFromStats(0, 1000, 0.8f) == StreamHealth::Bad);     // congested
    CHECK(HealthFromStats(0, 0, 0.0f) == StreamHealth::Good);       // no frames yet
}
