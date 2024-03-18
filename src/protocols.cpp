#include "protocols.h"
#include <string>

static ProtocolInfo s_infoList[] = {
    // protocol, label, output_id, service_id
    { "RTMP", "RTMP", "rtmp_output", "rtmp_custom" },
    { "SRT_RIST", "SRT/RIST", "ffmpeg_mpegts_muxer", "rtmp_custom" },
    { "WHIP", "WebRTC (WHIP)", "whip_output", "whip_custom" },
    { nullptr, nullptr, nullptr, nullptr }
};

class ProtocolInfosImpl: public ProtocolInfos {
public:
    const ProtocolInfo* GetInfo(const char* protocol) override {
        std::string_view to_find{ protocol };
        for(auto p = s_infoList; p->protocol; ++p) {
            if (to_find == p->protocol)
                return p;
        }
        return nullptr;
    }

    const ProtocolInfo* GetList() override {
        return s_infoList;
    }
};

ProtocolInfos* GetProtocolInfos() {
    static ProtocolInfosImpl impl_;
    return &impl_;
}
