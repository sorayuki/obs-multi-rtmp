#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <optional>
#include <type_traits>
#include <utility>

const char * const OBS_STREAMING_ENC_PLACEHOLDER = "<OBS_STREAMING_ENCODER>";
const char * const OBS_RECORDING_ENC_PLACEHOLDER = "<OBS_RECORDING_ENCODER>";
// Copy modes snapshot OBS *configured* settings into an independent,
// plugin-owned encoder. They work while OBS is idle and consume their
// own encoding resources (see EncoderSourceMode below).
const char * const OBS_STREAMING_COPY_PLACEHOLDER = "<OBS_STREAMING_COPY>";
const char * const OBS_RECORDING_COPY_PLACEHOLDER = "<OBS_RECORDING_COPY>";

// Explicit encoder source modes, independently for video and audio.
// "Share" borrows the live OBS encoder instance (requires the OBS source
// output to be active). "Copy" snapshots OBS configured settings into a
// new independent encoder (works while OBS is idle). "Custom" uses a
// plugin-owned encoder configuration (possibly shared with other targets
// via PopupShareMenu, which is a different feature from sharing a live
// OBS instance).
enum class EncoderSourceMode {
    ShareStreaming,
    ShareRecording,
    CopyStreaming,
    CopyRecording,
    Custom,
};

inline EncoderSourceMode ResolveEncoderSourceMode(const std::string_view &configId)
{
    if (configId == OBS_STREAMING_ENC_PLACEHOLDER)
        return EncoderSourceMode::ShareStreaming;
    if (configId == OBS_RECORDING_ENC_PLACEHOLDER)
        return EncoderSourceMode::ShareRecording;
    if (configId == OBS_STREAMING_COPY_PLACEHOLDER)
        return EncoderSourceMode::CopyStreaming;
    if (configId == OBS_RECORDING_COPY_PLACEHOLDER)
        return EncoderSourceMode::CopyRecording;
    return EncoderSourceMode::Custom;
}

inline EncoderSourceMode ResolveEncoderSourceMode(const std::optional<std::string> &configId)
{
    if (!configId.has_value() || configId->empty())
        return EncoderSourceMode::ShareStreaming;
    return ResolveEncoderSourceMode(std::string_view(*configId));
}

inline EncoderSourceMode ResolveEncoderSourceMode(const std::string &configId)
{
    return ResolveEncoderSourceMode(std::string_view(configId));
}

inline EncoderSourceMode ResolveEncoderSourceMode(const char *configId)
{
    if (!configId || !*configId)
        return EncoderSourceMode::ShareStreaming;
    return ResolveEncoderSourceMode(std::string_view(configId));
}

inline const char *EncoderSourceModeId(EncoderSourceMode mode)
{
    switch (mode) {
    case EncoderSourceMode::ShareStreaming:
        return OBS_STREAMING_ENC_PLACEHOLDER;
    case EncoderSourceMode::ShareRecording:
        return OBS_RECORDING_ENC_PLACEHOLDER;
    case EncoderSourceMode::CopyStreaming:
        return OBS_STREAMING_COPY_PLACEHOLDER;
    case EncoderSourceMode::CopyRecording:
        return OBS_RECORDING_COPY_PLACEHOLDER;
    case EncoderSourceMode::Custom:
    default:
        return "";
    }
}

inline bool IsShareEncoderSource(EncoderSourceMode mode)
{
    return mode == EncoderSourceMode::ShareStreaming ||
           mode == EncoderSourceMode::ShareRecording;
}

inline bool IsCopyEncoderSource(EncoderSourceMode mode)
{
    return mode == EncoderSourceMode::CopyStreaming ||
           mode == EncoderSourceMode::CopyRecording;
}

inline bool IsSpecialEncoder(const std::string_view& encoderId) {
    return encoderId == OBS_STREAMING_ENC_PLACEHOLDER ||
           encoderId == OBS_RECORDING_ENC_PLACEHOLDER ||
           encoderId == OBS_STREAMING_COPY_PLACEHOLDER ||
           encoderId == OBS_RECORDING_COPY_PLACEHOLDER;
}

// Legacy predicate: true only for live-instance sharing placeholders.
// Copy placeholders are *not* shared instances; they own an independent
// encoder snapshotted from OBS settings.
inline bool IsSharedObsEncoder(const std::string_view &encoderId)
{
    return encoderId == OBS_STREAMING_ENC_PLACEHOLDER ||
           encoderId == OBS_RECORDING_ENC_PLACEHOLDER;
}

inline bool IsCopyObsEncoder(const std::string_view &encoderId)
{
    return encoderId == OBS_STREAMING_COPY_PLACEHOLDER ||
           encoderId == OBS_RECORDING_COPY_PLACEHOLDER;
}

// Legacy migration rule: old profiles only ever stored the two Share
// placeholders or custom config IDs. They keep their exact meaning:
// Share stays Share, custom stays Custom. Nothing is reinterpreted as
// Copy on load.
inline EncoderSourceMode MigrateLegacyEncoderId(const std::string_view &storedId)
{
    return ResolveEncoderSourceMode(storedId);
}

// Audio tracks: UI shows 1-6, libobs uses 0-5.
inline constexpr int kObsAudioTrackCount = 6;

inline bool IsValidDisplayAudioTrack(int track)
{
    return track >= 1 && track <= kObsAudioTrackCount;
}

inline bool IsValidLibobsAudioTrack(int track)
{
    return track >= 0 && track < kObsAudioTrackCount;
}

inline int DisplayTrackToLibobs(int displayTrack)
{
    return displayTrack - 1;
}

inline int LibobsTrackToDisplay(int libobsTrack)
{
    return libobsTrack + 1;
}

// Pure codec-list check against an output's ';'-separated codec list.
// A null/empty list means "no restriction declared". A null/empty codec
// means "unknown, let creation report it". Never throws.
inline bool CodecSupportedByOutput(const std::string_view &supportedList,
                                   const std::string_view &codec)
{
    if (supportedList.empty())
        return true;
    if (codec.empty())
        return true;
    size_t pos = 0;
    while (pos <= supportedList.size()) {
        size_t end = supportedList.find(';', pos);
        if (end == std::string_view::npos)
            end = supportedList.size();
        if (supportedList.compare(pos, end - pos, codec) == 0)
            return true;
        if (end == supportedList.size())
            break;
        pos = end + 1;
    }
    return false;
}

template <typename T>
class WeakThis
{
    T* pointer_;
    std::weak_ptr<void> token_;

public:
    WeakThis(T* pointer, const std::shared_ptr<void>& token)
        : pointer_(pointer)
        , token_(token)
    {
    }

    T* Get() const
    {
        auto token = token_.lock();
        return token ? pointer_ : nullptr;
    }
};

#define OBS_MULTI_RTMP_DEFINE_WEAK_THIS() \
private: \
    std::shared_ptr<void> weak_this_token_ = std::make_shared<char>(); \
public: \
    auto GetWeakThis() { return WeakThis(this, weak_this_token_); } \
private:

// Calls the stored function when the guard goes out of scope, also on unwinding.
// Create it through finally() so the function type is deduced.
template <typename Fn>
class Finally final {
    Fn fn_;
    bool armed_ = true;

public:
    explicit Finally(Fn fn) : fn_(std::move(fn)) {}

    Finally(const Finally&) = delete;
    Finally& operator=(const Finally&) = delete;
    Finally(Finally&&) = delete;
    Finally& operator=(Finally&&) = delete;

    ~Finally() {
        if (armed_)
            fn_();
    }

    // Skip the pending call, e.g. when the scope has already handled it.
    void Dismiss() noexcept { armed_ = false; }
};

// Usage: auto g = finally([&] { cleanup(); });
template <typename Fn>
[[nodiscard]] inline Finally<typename std::decay<Fn>::type> finally(Fn&& fn)
{
    return Finally<typename std::decay<Fn>::type>(std::forward<Fn>(fn));
}
