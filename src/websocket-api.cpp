#include "pch.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>

#include "obs.hpp"
#include "json.hpp"

#include "obs-websocket-api.h"
#include "websocket-api.h"
#include "dock-registry.h"
#include "output-config.h"
#include "protocols.h"

namespace {

obs_websocket_vendor g_vendor = nullptr;

// Field names containing any of these (case-insensitive) are masked in
// get_target_config's response unless the caller explicitly asks to see
// them, so a careless script (or an AI agent logging tool output) doesn't
// casually leak a stream key.
const char* const kSensitiveFieldMarkers[] = { "key", "token", "password", "secret" };

bool IsSensitiveFieldName(const std::string& fieldName) {
    auto lower = fieldName;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (auto marker : kSensitiveFieldMarkers) {
        if (lower.find(marker) != std::string::npos)
            return true;
    }
    return false;
}

nlohmann::json RedactSecrets(const nlohmann::json& settings) {
    if (!settings.is_object())
        return settings;

    nlohmann::json out = nlohmann::json::object();
    for (auto it = settings.begin(); it != settings.end(); ++it)
        out[it.key()] = IsSensitiveFieldName(it.key()) ? nlohmann::json("***redacted***") : it.value();
    return out;
}

nlohmann::json RequestDataToJson(obs_data_t* request_data) {
    if (!request_data)
        return nlohmann::json::object();
    auto s = obs_data_get_json(request_data);
    if (!s)
        return nlohmann::json::object();
    try {
        auto j = nlohmann::json::parse(s);
        if (j.is_object())
            return j;
    } catch (...) {
    }
    return nlohmann::json::object();
}

void SetResponseFromJson(obs_data_t* response_data, const nlohmann::json& j) {
    if (!response_data)
        return;
    auto dumped = j.dump();
    OBSDataAutoRelease parsed = obs_data_create_from_json(dumped.c_str());
    if (parsed)
        obs_data_apply(response_data, parsed);
}

const char* StateName(PushWidget* target) {
    if (target->IsReconnecting())
        return "reconnecting";
    if (target->IsConnecting())
        return "connecting";
    if (target->IsRunning())
        return "live";
    return "stopped";
}

nlohmann::json TargetSummaryJson(PushWidget* target) {
    nlohmann::json j;
    j["id"] = target->GetTargetId();
    j["name"] = target->GetTargetName();
    j["protocol"] = target->GetProtocol();
    j["state"] = StateName(target);
    j["enabled"] = target->GetEnabled();
    j["sync_start"] = target->GetSyncStart();
    j["sync_stop"] = target->GetSyncStop();
    return j;
}

nlohmann::json TargetStatusJson(PushWidget* target) {
    auto j = TargetSummaryJson(target);
    j["last_error_code"] = target->GetLastErrorCode();
    j["duration_ms"] = target->GetDurationMs();
    j["bitrate_bps"] = target->GetBitrateBps();
    j["fps"] = target->GetFps();
    return j;
}

void OnListTargets(obs_data_t*, obs_data_t* response_data, void*) {
    GetGlobalService().RunInUIThreadBlocking([&]() {
        auto targets = nlohmann::json::array();
        for (auto target : GetAllStreamTargets())
            targets.push_back(TargetSummaryJson(target));

        nlohmann::json resp;
        resp["targets"] = targets;
        SetResponseFromJson(response_data, resp);
    });
}

void OnGetTargetStatus(obs_data_t* request_data, obs_data_t* response_data, void*) {
    auto req = RequestDataToJson(request_data);
    auto id = req.value("id", std::string());

    GetGlobalService().RunInUIThreadBlocking([&]() {
        nlohmann::json resp;
        auto target = FindStreamTargetById(id);
        if (!target)
            resp["error"] = "target_not_found";
        else
            resp = TargetStatusJson(target);
        SetResponseFromJson(response_data, resp);
    });
}

void OnStartTarget(obs_data_t* request_data, obs_data_t* response_data, void*) {
    auto req = RequestDataToJson(request_data);
    auto id = req.value("id", std::string());

    GetGlobalService().RunInUIThreadBlocking([&]() {
        nlohmann::json resp;
        auto target = FindStreamTargetById(id);
        if (!target) {
            resp["success"] = false;
            resp["error"] = "target_not_found";
        } else {
            target->StartStreaming();
            resp["success"] = true;
        }
        SetResponseFromJson(response_data, resp);
    });
}

void OnStopTarget(obs_data_t* request_data, obs_data_t* response_data, void*) {
    auto req = RequestDataToJson(request_data);
    auto id = req.value("id", std::string());

    GetGlobalService().RunInUIThreadBlocking([&]() {
        nlohmann::json resp;
        auto target = FindStreamTargetById(id);
        if (!target) {
            resp["success"] = false;
            resp["error"] = "target_not_found";
        } else {
            target->ForceStopStreaming();
            resp["success"] = true;
        }
        SetResponseFromJson(response_data, resp);
    });
}

void OnStartAllTargets(obs_data_t*, obs_data_t* response_data, void*) {
    GetGlobalService().RunInUIThreadBlocking([&]() {
        int count = 0;
        for (auto target : GetAllStreamTargets()) {
            target->StartStreaming();
            ++count;
        }
        nlohmann::json resp;
        resp["success"] = true;
        resp["count"] = count;
        SetResponseFromJson(response_data, resp);
    });
}

void OnStopAllTargets(obs_data_t*, obs_data_t* response_data, void*) {
    GetGlobalService().RunInUIThreadBlocking([&]() {
        int count = 0;
        for (auto target : GetAllStreamTargets()) {
            target->ForceStopStreaming();
            ++count;
        }
        nlohmann::json resp;
        resp["success"] = true;
        resp["count"] = count;
        SetResponseFromJson(response_data, resp);
    });
}

void OnSetTargetServiceSettings(obs_data_t* request_data, obs_data_t* response_data, void*) {
    auto req = RequestDataToJson(request_data);
    auto id = req.value("id", std::string());
    auto merge = req.value("merge", true);
    auto restartIfActive = req.value("restart_if_active", true);
    // Safety valve: validate id/shape and report what WOULD happen without
    // touching the target, so a caller can check before committing to a
    // change on a possibly-live stream.
    auto dryRun = req.value("dry_run", false);
    auto settingsIt = req.find("settings");

    GetGlobalService().RunInUIThreadBlocking([&]() {
        nlohmann::json resp;
        auto target = FindStreamTargetById(id);
        if (!target) {
            resp["success"] = false;
            resp["error"] = "target_not_found";
        } else if (settingsIt == req.end() || !settingsIt->is_object()) {
            resp["success"] = false;
            resp["error"] = "settings_must_be_an_object";
        } else if (dryRun) {
            resp["success"] = true;
            resp["dry_run"] = true;
            resp["would_restart"] = target->IsRunning() && restartIfActive;
        } else {
            bool wasRunning = target->IsRunning();
            bool ok = target->SetServiceSettings(*settingsIt, merge, restartIfActive);
            resp["success"] = ok;
            resp["restarted"] = ok && wasRunning && restartIfActive;
        }
        SetResponseFromJson(response_data, resp);
    });
}

void OnGetTargetConfig(obs_data_t* request_data, obs_data_t* response_data, void*) {
    auto req = RequestDataToJson(request_data);
    auto id = req.value("id", std::string());
    // Safety valve: secrets stay redacted unless explicitly requested.
    auto revealSecrets = req.value("reveal_secrets", false);

    GetGlobalService().RunInUIThreadBlocking([&]() {
        nlohmann::json resp;
        auto target = FindStreamTargetById(id);
        if (!target) {
            resp["error"] = "target_not_found";
        } else {
            resp = TargetSummaryJson(target);
            auto serviceSettings = target->GetServiceSettings();
            resp["service_settings"] = revealSecrets ? serviceSettings : RedactSecrets(serviceSettings);
            resp["output_settings"] = target->GetOutputSettings();
        }
        SetResponseFromJson(response_data, resp);
    });
}

void OnCreateTarget(obs_data_t* request_data, obs_data_t* response_data, void*) {
    auto req = RequestDataToJson(request_data);
    auto name = req.value("name", std::string());
    auto protocol = req.value("protocol", std::string("RTMP"));
    auto serviceSettings = req.value("service_settings", nlohmann::json::object());
    auto outputSettings = req.value("output_settings", nlohmann::json::object());
    auto syncStart = req.value("sync_start", false);
    auto syncStop = req.value("sync_stop", false);
    // Safety valve: a target created over the API starts out disabled
    // (excluded from Start all / sync-start) unless the caller explicitly
    // opts in, so a bad create_target call doesn't silently start pushing
    // to some place a second later.
    auto enabled = req.value("enabled", false);
    // Safety valve: validate without creating anything.
    auto dryRun = req.value("dry_run", false);

    nlohmann::json resp;

    if (!GetProtocolInfos()->GetInfo(protocol.c_str())) {
        resp["success"] = false;
        resp["error"] = "invalid_protocol";
        SetResponseFromJson(response_data, resp);
        return;
    }
    if (!serviceSettings.is_object() || !outputSettings.is_object()) {
        resp["success"] = false;
        resp["error"] = "settings_must_be_an_object";
        SetResponseFromJson(response_data, resp);
        return;
    }

    if (dryRun) {
        resp["success"] = true;
        resp["dry_run"] = true;
        SetResponseFromJson(response_data, resp);
        return;
    }

    GetGlobalService().RunInUIThreadBlocking([&]() {
        auto target = std::make_shared<OutputTargetConfig>();
        target->id = GenerateId(GlobalMultiOutputConfig());
        target->name = name.empty() ? obs_module_text("NewStreaming") : name;
        target->protocol = protocol;
        target->serviceParam = serviceSettings;
        target->outputParam = outputSettings;
        target->syncStart = syncStart;
        target->syncStop = syncStop;
        target->enabled = enabled;

        auto pushWidget = CreateStreamTarget(target);
        resp["success"] = pushWidget != nullptr;
        if (pushWidget)
            resp["id"] = target->id;
        SetResponseFromJson(response_data, resp);
    });
}

void OnDeleteTarget(obs_data_t* request_data, obs_data_t* response_data, void*) {
    auto req = RequestDataToJson(request_data);
    auto id = req.value("id", std::string());
    // Safety valve: deletion is refused unless the caller explicitly
    // confirms, and always refused while the target is live (stop_target
    // must be called first) - a delete_target call can't silently kill a
    // running broadcast.
    auto confirm = req.value("confirm", false);

    GetGlobalService().RunInUIThreadBlocking([&]() {
        nlohmann::json resp;
        auto target = FindStreamTargetById(id);
        if (!target) {
            resp["success"] = false;
            resp["error"] = "target_not_found";
        } else if (!confirm) {
            resp["success"] = false;
            resp["error"] = "confirmation_required";
            resp["hint"] = "Pass \"confirm\": true to delete this target.";
        } else if (target->IsRunning()) {
            resp["success"] = false;
            resp["error"] = "target_is_live";
            resp["hint"] = "Call stop_target first, then retry delete_target.";
        } else {
            resp["success"] = DeleteStreamTarget(id);
        }
        SetResponseFromJson(response_data, resp);
    });
}

void OnSetTargetEnabled(obs_data_t* request_data, obs_data_t* response_data, void*) {
    auto req = RequestDataToJson(request_data);
    auto id = req.value("id", std::string());
    auto enabled = req.value("enabled", true);

    GetGlobalService().RunInUIThreadBlocking([&]() {
        nlohmann::json resp;
        auto target = FindStreamTargetById(id);
        if (!target) {
            resp["success"] = false;
            resp["error"] = "target_not_found";
        } else {
            target->SetEnabled(enabled);
            resp["success"] = true;
        }
        SetResponseFromJson(response_data, resp);
    });
}

// Defined after kRequests below (it lists kRequests's own contents), but
// referenced by it, so only forward-declared here - taking a function's
// address doesn't require its body to be visible yet.
void OnListCapabilities(obs_data_t*, obs_data_t*, void*);

struct VendorRequest {
    const char* name;
    const char* description;
    obs_websocket_request_callback_function callback;
};

// list_capabilities returns this table's name+description for every entry
// (including itself), so a caller - especially an AI agent - can discover
// the whole API from one request instead of needing WEBSOCKET_API.md
// pre-loaded into its context.
const VendorRequest kRequests[] = {
    { "list_targets", "List every target with its id, name, protocol, state, enabled flag, and sync flags.", &OnListTargets },
    { "get_target_status", "Params: id. Live status (state, duration, bitrate, fps, last error) for one target.", &OnGetTargetStatus },
    { "get_target_config", "Params: id, reveal_secrets (default false). Full saved config for one target; service_settings fields matching key/token/password/secret are redacted unless reveal_secrets is true.", &OnGetTargetConfig },
    { "start_target", "Params: id. Starts one target (no-op if disabled or already running).", &OnStartTarget },
    { "stop_target", "Params: id. Force-stops one target immediately, no confirmation dialog.", &OnStopTarget },
    { "start_all_targets", "Starts every enabled target.", &OnStartAllTargets },
    { "stop_all_targets", "Force-stops every target.", &OnStopAllTargets },
    { "set_target_service_settings", "Params: id, settings, merge (default true), restart_if_active (default true), dry_run (default false). Updates a target's server/key etc.; dry_run validates without applying.", &OnSetTargetServiceSettings },
    { "set_target_enabled", "Params: id, enabled. Enables/disables a target; disabling a live target force-stops it.", &OnSetTargetEnabled },
    { "create_target", "Params: name, protocol, service_settings, output_settings, sync_start, sync_stop, enabled (default false), dry_run (default false). Adds a new target; disabled by default until you explicitly enable it.", &OnCreateTarget },
    { "delete_target", "Params: id, confirm (must be true). Deletes a target; refused if it's currently live (stop_target it first).", &OnDeleteTarget },
    { "list_capabilities", "Lists every vendor request this plugin registers, with a one-line description of each.", &OnListCapabilities },
};

void OnListCapabilities(obs_data_t*, obs_data_t* response_data, void*) {
    nlohmann::json resp;
    resp["vendor"] = "obs-multi-rtmp";
    resp["docs"] = "https://github.com/sorayuki/obs-multi-rtmp/blob/master/WEBSOCKET_API.md";
    resp["events"] = nlohmann::json::array({ "target_state_changed" });

    auto requests = nlohmann::json::array();
    for (auto& req : kRequests) {
        nlohmann::json r;
        r["name"] = req.name;
        r["description"] = req.description;
        requests.push_back(r);
    }
    resp["requests"] = requests;

    SetResponseFromJson(response_data, resp);
}

} // namespace

void RegisterWebsocketVendor() {
    g_vendor = obs_websocket_register_vendor("obs-multi-rtmp");
    if (!g_vendor) {
        blog(LOG_INFO, TAG "obs-websocket not available; external control API disabled.");
        return;
    }

    for (auto& req : kRequests)
        obs_websocket_vendor_register_request(g_vendor, req.name, req.callback, nullptr);

    blog(LOG_INFO, TAG "obs-websocket vendor API registered (vendor: \"obs-multi-rtmp\").");
}

void NotifyTargetStateChanged(const std::string& id, const std::string& name, const std::string& state, int lastErrorCode) {
    if (!g_vendor)
        return;

    nlohmann::json data;
    data["id"] = id;
    data["name"] = name;
    data["state"] = state;
    data["last_error_code"] = lastErrorCode;

    auto dumped = data.dump();
    OBSDataAutoRelease event_data = obs_data_create_from_json(dumped.c_str());
    if (!event_data)
        return;

    obs_websocket_vendor_emit_event(g_vendor, "target_state_changed", event_data);
}
