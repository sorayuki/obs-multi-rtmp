#pragma once
#include <string>
#include <vector>
#include <json.hpp>

// This header is intentionally logger-free and OBS/Qt-free: it must stay
// buildable in a plain host toolchain (unit tests) with no <obs.h>, no
// obs-frontend-api, and no Qt. The UI wiring (combo box, signal handling)
// lives in edit-widget.cpp, which calls into these pure helpers.

struct PlatformPreset { std::string name; std::string serverUrl; };

// Sentinel preset name: the caller (edit-widget.cpp's preset combo handler)
// treats this entry specially and leaves the "server" field completely
// untouched when it's selected -- it never calls ApplyPresetServer for it.
// Every OTHER preset, including label-only ones with an empty serverUrl
// (Kick/TikTok -- see the comment in platform-presets.cpp), is applied via
// ApplyPresetServer, which always writes "server". Compare against this
// constant instead of a raw "Custom" string literal so the sentinel has one
// source of truth.
inline constexpr const char* kCustomPresetName = "Custom";

const std::vector<PlatformPreset>& GetPlatformPresets();

// Returns `serviceParam` with its "server" field set to `serverUrl` (all
// other fields preserved). This ALWAYS writes "server", including setting
// it to an empty string when `serverUrl` is empty -- i.e. it CLEARS any
// previously-set server rather than leaving it alone. That's deliberate: a
// caller cycling through presets (e.g. YouTube -> Kick) must never end up
// with a stale server URL left over from a different platform. The one
// preset that should leave "server" untouched is "Custom" (kCustomPresetName)
// -- callers implement that by simply not invoking this function for it,
// not by relying on an empty-string no-op here. A null or non-object
// `serviceParam` is treated as an empty object before applying the update.
nlohmann::json ApplyPresetServer(nlohmann::json serviceParam, const std::string& serverUrl);
