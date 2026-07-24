#include "platform-presets.h"

const std::vector<PlatformPreset>& GetPlatformPresets() {
    // NOTE: these URLs were checked against public documentation/help-center
    // sources at implementation time (2026-07) and are believed to be
    // current, but streaming platforms change ingest infrastructure without
    // notice -- verify against the platform's own dashboard/docs if a preset
    // stops working, and fall back to "Custom" with a manually entered URL.
    //
    // Twitch, YouTube, and Facebook publish a single, stable, account-
    // independent RTMP(S) ingest endpoint, so those presets carry a real
    // server URL below.
    //
    // Kick and TikTok do NOT: Kick issues a per-account/region ingest host
    // (format "rtmps://<hash>.global-contribute.live-video.net:443/app",
    // where <hash> differs per streamer/region -- only visible in the
    // user's own Kick creator dashboard), and TikTok LIVE Studio generates a
    // temporary RTMP server + key pair per session that expires after ~2
    // hours (and streaming access itself is gated behind TikTok's Creator
    // Network / follower-count requirements). Hardcoding a single URL for
    // either would be actively wrong for most users, so those two presets
    // intentionally ship an empty serverUrl: selecting them leaves the
    // "server" field untouched (same as "Custom"), just labeled for
    // convenience. The user must copy their own server URL from the
    // platform's dashboard.
    static const std::vector<PlatformPreset> presets = {
        {"Custom", ""},
        {"Twitch", "rtmp://live.twitch.tv/app"},
        {"YouTube", "rtmp://a.rtmp.youtube.com/live2"},
        {"Kick", ""},
        {"TikTok", ""},
        {"Facebook", "rtmps://live-api-s.facebook.com:443/rtmp"},
    };
    return presets;
}

nlohmann::json ApplyPresetServer(nlohmann::json serviceParam, const std::string& serverUrl) {
    if (serviceParam.is_null() || !serviceParam.is_object())
        serviceParam = nlohmann::json::object();
    if (!serverUrl.empty())
        serviceParam["server"] = serverUrl;
    return serviceParam;
}
