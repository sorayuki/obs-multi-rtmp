#pragma once

#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

const char * const OBS_STREAMING_ENC_PLACEHOLDER = "<OBS_STREAMING_ENCODER>";
const char * const OBS_RECORDING_ENC_PLACEHOLDER = "<OBS_RECORDING_ENCODER>";

inline bool IsSpecialEncoder(const std::string_view& encoderId) {
    return encoderId == OBS_STREAMING_ENC_PLACEHOLDER || encoderId == OBS_RECORDING_ENC_PLACEHOLDER;
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
