#pragma once

#include <string_view>
#include <type_traits>
#include <utility>

const char * const OBS_STREAMING_ENC_PLACEHOLDER = "<OBS_STREAMING_ENCODER>";
const char * const OBS_RECORDING_ENC_PLACEHOLDER = "<OBS_RECORDING_ENCODER>";

inline bool IsSpecialEncoder(const std::string_view& encoderId) {
    return encoderId == OBS_STREAMING_ENC_PLACEHOLDER || encoderId == OBS_RECORDING_ENC_PLACEHOLDER;
}

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
