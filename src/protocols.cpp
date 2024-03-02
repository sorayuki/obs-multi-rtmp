#include "protocols.h"

    QStringList protocol_labels = {"RTMP", "SRT/RIST", "WebRTC (WHIP)"};
    QStringList protocol_values = {"RTMP", "SRT_RIST", "WHIP"};

    static const char *GetOutputID(const char *protocol) {
        if (strncmp("SRT",  protocol, 3) == 0)  return "ffmpeg_mpegts_muxer";
        if (strncmp("WHIP", protocol, 4) == 0)  return "whip_output";
        return "rtmp_output";
    }
    
    static const char *GetServiceID(const char *protocol) {
        if (strncmp("WHIP", protocol, 4) == 0) return "whip_custom";
        return "rtmp_custom";
    }
