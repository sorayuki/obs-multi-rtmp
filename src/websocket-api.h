#pragma once

#include <string>

// Bridges this plugin's stream targets to obs-websocket's vendor request API
// (https://github.com/obsproject/obs-websocket), so external automation
// (scripts driving OBS through obs-websocket) can list targets, start/stop
// them individually, read per-target status, and update a target's stream
// key/server without going through this plugin's Qt dock. Every request
// this registers is documented in WEBSOCKET_API.md.
//
// obs-websocket may or may not be installed; every function here is a
// silent no-op in that case.

// Registers the "obs-multi-rtmp" vendor and its requests with obs-websocket.
// Must be called from obs_module_post_load(), per obs-websocket-api.h's own
// requirement that vendors register only after all plugins have finished
// loading.
void RegisterWebsocketVendor();

// Emits a "target_state_changed" vendor event: {id, name, state,
// last_error_code}, where state is one of "connecting" / "live" /
// "reconnecting" / "stopped". Safe to call before RegisterWebsocketVendor()
// runs or when obs-websocket isn't installed (both are silently ignored).
// Must be called from the UI thread.
void NotifyTargetStateChanged(const std::string& id, const std::string& name, const std::string& state, int lastErrorCode);
