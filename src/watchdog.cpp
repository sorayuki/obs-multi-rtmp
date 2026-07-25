#include "watchdog.h"

WatchdogAction WatchdogDecide(WatchdogState& state, int stopCode, bool manualStop, int maxRestarts) {
    if (manualStop || stopCode == 0)
        return { false, {} };
    if (state.retries >= maxRestarts)
        return { false, {} };
    static const int backoff[] = { 5000, 15000, 30000 };
    int idx = state.retries < 3 ? state.retries : 2;
    ++state.retries;
    return { true, std::chrono::milliseconds(backoff[idx]) };
}
