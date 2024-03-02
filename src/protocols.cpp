#include "protocols.h"
#include <string>

    extern const QStringList protocol_labels = {"RTMP", "SRT/RIST", "WebRTC (WHIP)"};
    extern const QStringList protocol_values = {"RTMP", "SRT_RIST", "WHIP"};

    const char *GetOutputID(std::string protocol) {
        if (protocol == "SRT_RIST") return "ffmpeg_mpegts_muxer";
        if (protocol == "WHIP")     return "whip_output";
        return "rtmp_output";
    }
    
    const char *GetServiceID(std::string protocol) {
        if (protocol == "WHIP") return "whip_custom";
        return "rtmp_custom";
    }
