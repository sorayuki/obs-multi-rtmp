#pragma once
#include <QString>
class QSystemTrayIcon;

class Notifier {
public:
    static Notifier& Instance();
    void NotifyFailure(const QString& targetName, const QString& reason);
private:
    Notifier() = default;
    Notifier(const Notifier&) = delete;
    Notifier& operator=(const Notifier&) = delete;
    QSystemTrayIcon* tray_ = nullptr;
    void ensureTray();
};
