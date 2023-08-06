#include "pch.h"

class PushWidget : virtual public QWidget {
public:
    virtual ~PushWidget() {}
    virtual bool ShowEditDlg() = 0;
    virtual void StartStreaming() = 0;
    virtual void StopStreaming() = 0;
    virtual void OnOBSEvent(obs_frontend_event ev) = 0;
};

PushWidget* createPushWidget(const std::string& targetId, QWidget* parent = 0);
