#pragma once

#include <functional>

class GlobalService
{
public:
    ~GlobalService() {}
    virtual bool RunInUIThread(std::function<void()> task) = 0;
    // Like RunInUIThread, but blocks the calling thread until `task` has
    // finished running on the UI thread (or runs `task` directly if already
    // called from the UI thread). Used by the obs-websocket vendor API so a
    // request handler, invoked from obs-websocket's own thread, can safely
    // touch Qt widgets and hand back a real, synchronous result.
    virtual bool RunInUIThreadBlocking(std::function<void()> task) = 0;
};

GlobalService& GetGlobalService();
