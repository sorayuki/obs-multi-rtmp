#pragma once

#include <cstdint>
#include <string>

#include "json.hpp"

class PushWidget : virtual public QWidget {
public:
    virtual ~PushWidget() {}
    virtual bool ShowEditDlg() = 0;
    virtual void StartStreaming() = 0;
    virtual void StopStreaming() = 0;
    // Stops immediately, skipping the "drop delayed frames?" confirmation
    // dialog StopStreaming() shows when a stream delay is configured. Used
    // for headless control (e.g. the obs-websocket vendor API) where no one
    // is present to answer a modal dialog.
    virtual void ForceStopStreaming() = 0;
    virtual void OnOBSEvent(obs_frontend_event ev) = 0;
    virtual QPushButton* GetDeleteButton() = 0;

    // Read-only target info and live status, used by the obs-websocket
    // vendor API (see websocket-api.cpp) to answer list/status requests.
    virtual std::string GetTargetId() = 0;
    virtual std::string GetTargetName() = 0;
    virtual std::string GetProtocol() = 0;
    virtual bool GetSyncStart() = 0;
    virtual bool GetSyncStop() = 0;
    // When false, StartStreaming() refuses to start this target (see the
    // "enabled" field on OutputTargetConfig for why). Setting this to false
    // while the target is live also force-stops it.
    virtual bool GetEnabled() = 0;
    virtual void SetEnabled(bool enabled) = 0;
    virtual bool IsRunning() = 0;
    virtual bool IsConnecting() = 0;
    virtual bool IsReconnecting() = 0;
    virtual int GetLastErrorCode() = 0;
    virtual uint64_t GetDurationMs() = 0;
    virtual uint64_t GetBitrateBps() = 0;
    virtual double GetFps() = 0;

    // Merges (or, if merge=false, replaces) the given fields into the
    // target's service settings (e.g. "server"/"key" for RTMP) and saves
    // the config. If the target is currently live and restartIfActive is
    // true, it is stopped and immediately restarted so the new settings
    // take effect right away (e.g. swapping in a rolled-over YouTube stream
    // key without needing to reopen the edit dialog).
    virtual bool SetServiceSettings(const nlohmann::json& patch, bool merge, bool restartIfActive) = 0;
};

PushWidget* createPushWidget(const std::string& targetId, QWidget* parent = 0);
