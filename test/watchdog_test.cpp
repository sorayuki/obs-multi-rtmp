#include "doctest.h"
#include "watchdog.h"

TEST_CASE("no restart on manual stop") {
    WatchdogState s;
    auto a = WatchdogDecide(s, 0, /*manualStop=*/true, 5);
    CHECK(a.restart == false);
}

TEST_CASE("no restart on clean stop") {
    WatchdogState s;
    CHECK(WatchdogDecide(s, 0, false, 5).restart == false);
}

TEST_CASE("restarts with backoff up to the cap") {
    WatchdogState s;
    auto a1 = WatchdogDecide(s, -2, false, 3);
    CHECK(a1.restart == true);
    CHECK(a1.delay == std::chrono::milliseconds(5000));
    auto a2 = WatchdogDecide(s, -2, false, 3);
    CHECK(a2.delay == std::chrono::milliseconds(15000));
    auto a3 = WatchdogDecide(s, -2, false, 3);
    CHECK(a3.delay == std::chrono::milliseconds(30000));
    auto a4 = WatchdogDecide(s, -2, false, 3);
    CHECK(a4.restart == false); // cap reached
}
