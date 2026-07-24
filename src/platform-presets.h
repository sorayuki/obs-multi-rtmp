#pragma once
#include <string>
#include <vector>
#include <json.hpp>

// This header is intentionally logger-free and OBS/Qt-free: it must stay
// buildable in a plain host toolchain (unit tests) with no <obs.h>, no
// obs-frontend-api, and no Qt. The UI wiring (combo box, signal handling)
// lives in edit-widget.cpp, which calls into these pure helpers.

struct PlatformPreset { std::string name; std::string serverUrl; };

const std::vector<PlatformPreset>& GetPlatformPresets();

// Returns `serviceParam` with its "server" field set to `serverUrl` (all
// other fields preserved). If `serverUrl` is empty (the "Custom" preset),
// `serviceParam` is returned unchanged. A null or non-object `serviceParam`
// is treated as an empty object before applying the update.
nlohmann::json ApplyPresetServer(nlohmann::json serviceParam, const std::string& serverUrl);
