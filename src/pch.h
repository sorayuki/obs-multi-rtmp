#pragma once

#include <memory>
#include <string>

#include <QMainWindow>
#include <QDockWidget>
#include <QWidget>
#include <QLabel>
#include <QString>
#include <QPushButton>
#include <QScrollArea>
#include <QGridLayout>
#include <QEvent>
#include <QThread>
#include <QLineEdit>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QComboBox>
#include <QGroupBox>
#include <QAction>

#include "obs-multi-rtmp.h"
#include "obs-module.h"
#include "obs-frontend-api.h"
#include "util/config-file.h"

inline std::string tostdu8(const QString& qs)
{
    auto b = qs.toUtf8();
    return std::string(b.begin(), b.end());
}


extern QThread* g_uiThread;

template<class T>
bool RunInUIThread(T&& func)
{
    if (g_uiThread == nullptr)
        return false;
    QMetaObject::invokeMethod(g_uiThread, [func = std::move(func)]() {
        func();
    });
    return true;
}