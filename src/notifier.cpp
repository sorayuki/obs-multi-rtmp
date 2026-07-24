#include "notifier.h"
#include <QSystemTrayIcon>
#include <QApplication>
#include <QIcon>

Notifier& Notifier::Instance() {
    static Notifier n;
    return n;
}

void Notifier::ensureTray() {
    if (tray_ || !QSystemTrayIcon::isSystemTrayAvailable())
        return;
    tray_ = new QSystemTrayIcon(QApplication::windowIcon());
    tray_->show();
}

void Notifier::NotifyFailure(const QString& targetName, const QString& reason) {
    ensureTray();
    if (tray_) {
        tray_->showMessage(QStringLiteral("obs-multi-rtmp"),
            targetName + ": " + reason, QSystemTrayIcon::Warning, 5000);
    }
}
