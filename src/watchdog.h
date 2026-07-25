#pragma once
#include <chrono>

struct WatchdogState { int retries = 0; };
struct WatchdogAction { bool restart; std::chrono::milliseconds delay; };
WatchdogAction WatchdogDecide(WatchdogState& state, int stopCode, bool manualStop, int maxRestarts);
