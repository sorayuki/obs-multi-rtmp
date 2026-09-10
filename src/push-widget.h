#pragma once
#include "pch.h"
#include "output-config.h"

class PushWidget : public QWidget {
    Q_OBJECT
public:
    PushWidget(QWidget* parent = nullptr) : QWidget(parent) {}
    virtual ~PushWidget() {}
    virtual bool ShowEditDlg() = 0;
    virtual void StartStreaming() = 0;
    virtual void StopStreaming() = 0;
    virtual void OnOBSEvent(obs_frontend_event ev) = 0;
    virtual QPushButton* GetDeleteButton() = 0;

    // Websocket access methods
    virtual QString GetTargetId() const = 0;
    virtual QString GetTargetName() const = 0;
    virtual QString GetStatusText() const = 0;
    virtual bool IsRunning() const = 0;
    virtual void UpdateUI() = 0;
    virtual OutputTargetConfigPtr GetConfig() const = 0;
    virtual bool IsEditing() const = 0;
};

PushWidget* createPushWidget(const std::string& targetId, QWidget* parent = 0);
