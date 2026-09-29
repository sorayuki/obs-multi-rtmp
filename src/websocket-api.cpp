#include "pch.h"

#include <cassert>
#include <cstring>

#include "obs.hpp"
#include "json.hpp"

#include "obs-websocket-api.h"
#include "websocket-api.h"
#include "dock-registry.h"

namespace {

obs_websocket_vendor g_vendor = nullptr;

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
        } else {
            bool wasRunning = target->IsRunning();
            bool ok = target->SetServiceSettings(*settingsIt, merge, restartIfActive);
            resp["success"] = ok;
            resp["restarted"] = ok && wasRunning && restartIfActive;
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

struct VendorRequest {
    const char* name;
    obs_websocket_request_callback_function callback;
};

const VendorRequest kRequests[] = {
    { "list_targets", &OnListTargets },
    { "get_target_status", &OnGetTargetStatus },
    { "start_target", &OnStartTarget },
    { "stop_target", &OnStopTarget },
    { "start_all_targets", &OnStartAllTargets },
    { "stop_all_targets", &OnStopAllTargets },
    { "set_target_service_settings", &OnSetTargetServiceSettings },
    { "set_target_enabled", &OnSetTargetEnabled },
};

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
