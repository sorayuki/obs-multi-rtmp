#pragma once

#include <string>
#include <string_view>

struct ProtocolInfo {
    const char* protocol;
    const char* label;
    const char* outputId;
    const char* serviceId;
};

class ProtocolInfos {
public:
    virtual const ProtocolInfo* GetInfo(const char* protocol) = 0;
    virtual const ProtocolInfo* GetList() = 0;
};

ProtocolInfos* GetProtocolInfos();
