#include "pch.h"

#include <string>
#include <vector>

struct MultiOutputConfig;
struct GlobalConfigUpdateContext;

class PushWidget : virtual public QWidget {
public:
    virtual ~PushWidget() {}
    virtual void StartStreaming() = 0;
    virtual void StopStreaming() = 0;
    virtual bool IsRunning() = 0;
    virtual bool IsBusy() = 0;
    virtual const std::string& GetTargetId() const = 0;
    virtual void UpdateConfig(const MultiOutputConfig& config, GlobalConfigUpdateContext& context) = 0;
    virtual void OnOBSEvent(obs_frontend_event ev) = 0;
    // When false, StartStreaming() refuses to start this target (see the
    // "enabled" field on OutputTargetConfig for why). Setting this to false
    // while the target is live also force-stops it.
    virtual bool GetEnabled() = 0;
    virtual void SetEnabled(bool enabled) = 0;
};

PushWidget* createPushWidget(const std::string& targetId, QWidget* parent = 0);
