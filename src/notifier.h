#pragma once
#include <QString>
class QSystemTrayIcon;

class Notifier {
public:
    static Notifier& Instance();
    void NotifyFailure(const QString& targetName, const QString& reason);
private:
    Notifier() = default;
    QSystemTrayIcon* tray_ = nullptr;
    void ensureTray();
};
