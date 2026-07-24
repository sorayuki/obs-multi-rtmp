#include "stream-health.h"

StreamHealth HealthFromStats(uint64_t framesDropped, uint64_t framesTotal, float congestion) {
    double pct = framesTotal > 0 ? (100.0 * framesDropped / framesTotal) : 0.0;
    if (pct > 5.0 || congestion >= 0.7f) return StreamHealth::Bad;
    if (pct >= 1.0 || congestion >= 0.3f) return StreamHealth::Warn;
    return StreamHealth::Good;
}
