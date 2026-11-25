#include "encoder-source.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

const char *EncoderResolveErrorKey(EncoderResolveError err)
{
    switch (err) {
    case EncoderResolveError::None:
        return "";
    case EncoderResolveError::SharedUnavailable:
        return "Error.SharedEncoderUnavailable";
    case EncoderResolveError::CopyUnresolvable:
        return "Error.CopyEncoderUnresolvable";
    case EncoderResolveError::UnsupportedVideoCodec:
        return "Error.UnsupportedVideoCodec";
    case EncoderResolveError::UnsupportedAudioCodec:
        return "Error.UnsupportedAudioCodec";
    case EncoderResolveError::MissingVideoConfig:
        return "Error.MissingVideoConfig";
    case EncoderResolveError::MissingAudioConfig:
        return "Error.MissingAudioConfig";
    case EncoderResolveError::InvalidVideoConfig:
        return "Error.InvalidVideoConfig";
    case EncoderResolveError::InvalidAudioConfig:
        return "Error.InvalidAudioConfig";
    case EncoderResolveError::MissingTrack:
        return "Error.MissingAudioTrack";
    case EncoderResolveError::StartOutputFailed:
        return "Error.StartOutput";
    }
    return "Error.Unknown";
}

static std::string Lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

std::optional<std::string> MapSimpleVideoEncoder(const std::string &shortName)
{
    const std::string n = Lower(shortName);
    if (n == "x264" || n == "obs_x264")
        return std::string("obs_x264");
    // Hardware short names ("nvenc", "qsv", "amd", "apple_h264", ...) map to
    // platform/driver-specific ids that differ between OBS builds. Guessing
    // one would silently pick the wrong encoder, so report unresolvable and
    // let the user pick Custom.
    return std::nullopt;
}

std::optional<std::string> MapSimpleAudioEncoder(const std::string &shortName)
{
    const std::string n = Lower(shortName);
    if (n == "aac" || n == "ffmpeg_aac" || n == "libfdk_aac" || n == "libfdk_aac")
        return std::string("ffmpeg_aac");
    return std::nullopt;
}

static bool IsAdvanced(const ObsProfileView &p)
{
    return Lower(p.outputMode) != "simple";
}

std::optional<std::string> ResolveCopyVideoEncoderId(const ObsProfileView &profile,
                                                     EncoderSourceMode mode)
{
    const bool streaming = (mode == EncoderSourceMode::CopyStreaming);
    const bool recording = (mode == EncoderSourceMode::CopyRecording);
    if (!streaming && !recording)
        return std::nullopt;
    if (IsAdvanced(profile)) {
        if (streaming) {
            if (profile.advStreamVideo.empty() || profile.advStreamVideo == "none")
                return std::nullopt;
            return profile.advStreamVideo;
        }
        // Recording that reuses the streaming encoder snapshots the same id.
        if (profile.recUsesStreamEncoder || profile.advRecVideo.empty() ||
            profile.advRecVideo == "none")
            return profile.advStreamVideo.empty() ? std::nullopt
                                                 : std::optional<std::string>(profile.advStreamVideo);
        // FFmpeg/custom recording outputs ("ffmpeg_*", empty ids, file-only
        // pipelines) cannot be reconstructed as an RTMP encoder reliably.
        if (profile.advRecVideo.rfind("ffmpeg_", 0) == 0)
            return std::nullopt;
        return profile.advRecVideo;
    }
    // Simple mode.
    if (streaming)
        return profile.simpleStreamVideo.empty()
                   ? std::nullopt
                   : MapSimpleVideoEncoder(profile.simpleStreamVideo);
    // Simple recording mirrors the streaming encoder in most setups; a
    // distinct simpleRecVideo is honoured when present.
    if (!profile.simpleRecVideo.empty())
        return MapSimpleVideoEncoder(profile.simpleRecVideo);
    return profile.simpleStreamVideo.empty()
               ? std::nullopt
               : MapSimpleVideoEncoder(profile.simpleStreamVideo);
}

std::optional<std::string> ResolveCopyAudioEncoderId(const ObsProfileView &profile,
                                                     EncoderSourceMode mode)
{
    const bool streaming = (mode == EncoderSourceMode::CopyStreaming);
    const bool recording = (mode == EncoderSourceMode::CopyRecording);
    if (!streaming && !recording)
        return std::nullopt;
    if (IsAdvanced(profile)) {
        if (streaming) {
            if (profile.advStreamAudio.empty() || profile.advStreamAudio == "none")
                return std::nullopt;
            return profile.advStreamAudio;
        }
        if (profile.recUsesStreamEncoder || profile.advRecAudio.empty() ||
            profile.advRecAudio == "none")
            return profile.advStreamAudio.empty() ? std::nullopt
                                                 : std::optional<std::string>(profile.advStreamAudio);
        if (profile.advRecAudio.rfind("ffmpeg_", 0) == 0)
            return std::nullopt;
        return profile.advRecAudio;
    }
    if (streaming)
        return profile.simpleStreamAudio.empty()
                   ? std::nullopt
                   : MapSimpleAudioEncoder(profile.simpleStreamAudio);
    if (!profile.simpleRecAudio.empty())
        return MapSimpleAudioEncoder(profile.simpleRecAudio);
    return profile.simpleStreamAudio.empty()
               ? std::nullopt
               : MapSimpleAudioEncoder(profile.simpleStreamAudio);
}

std::optional<std::string> ReadEncoderSnapshotJson(const std::string &profileDir,
                                                  const char *fileName)
{
    if (profileDir.empty() || !fileName || !*fileName)
        return std::nullopt;
    std::string path = profileDir;
    if (!path.empty() && path.back() != '/' && path.back() != '\\')
        path += '/';
    path += fileName;
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return std::string("{}");
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string text = ss.str();
    if (text.empty())
        return std::string("{}");
    return text;
}

std::optional<CopiedEncoderSpec> SnapshotCopyEncoder(const ObsProfileView &profile,
                                                    const std::string &profileDir,
                                                    EncoderSourceMode mode,
                                                    bool isVideo,
                                                    EncoderResolveError *error)
{
    auto setErr = [&](EncoderResolveError e) {
        if (error)
            *error = e;
    };
    std::optional<std::string> id = isVideo ? ResolveCopyVideoEncoderId(profile, mode)
                                            : ResolveCopyAudioEncoderId(profile, mode);
    if (!id.has_value() || id->empty()) {
        setErr(EncoderResolveError::CopyUnresolvable);
        return std::nullopt;
    }
    const char *file = (mode == EncoderSourceMode::CopyStreaming) ? "streamEncoder.json"
                                                                  : "recordEncoder.json";
    // Recording snapshots fall back to the streaming settings file when the
    // recording reuses the streaming encoder and has no file of its own.
    std::optional<std::string> json = ReadEncoderSnapshotJson(profileDir, file);
    if (!json.has_value()) {
        setErr(EncoderResolveError::CopyUnresolvable);
        return std::nullopt;
    }
    if (*json == "{}" && mode == EncoderSourceMode::CopyRecording && profile.recUsesStreamEncoder) {
        if (auto fallback = ReadEncoderSnapshotJson(profileDir, "streamEncoder.json"))
            json = fallback;
    }
    setErr(EncoderResolveError::None);
    return CopiedEncoderSpec{*id, *json};
}
