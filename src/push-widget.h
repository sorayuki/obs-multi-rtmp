#include "pch.h"

class PushWidget : virtual public QWidget {
public:
    virtual ~PushWidget() {}
    virtual QJsonObject Config() = 0;
    virtual void Stop() = 0;
    virtual bool ShowEditDlg() = 0;
};

PushWidget* createPushWidget(QJsonObject conf, QWidget* parent = 0);
