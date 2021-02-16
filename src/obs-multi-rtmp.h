#pragma once

#include <functional>

class GlobalService
{
public:
    ~GlobalService() {}
    virtual void SaveConfig() = 0;
    virtual bool RunInUIThread(std::function<void()> task) = 0;
};

GlobalService& GetGlobalService();
