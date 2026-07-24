#pragma once
#include <cstdint>

enum class StreamHealth { Good, Warn, Bad };
StreamHealth HealthFromStats(uint64_t framesDropped, uint64_t framesTotal, float congestion);
