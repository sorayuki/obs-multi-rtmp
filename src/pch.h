#pragma once

#include <memory>
#include <string>
#include <optional>
#include <type_traits>

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
#include <QTabWidget>
#include <QLineEdit>
#include <QTimer>
#include <QMessageBox>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QAction>

#include "obs-multi-rtmp.h"
#include "obs-module.h"
#include "obs-frontend-api.h"
#include "util/config-file.h"

#define TAG "[obs-multi-rtmp] "

 // Disable the warning about declaration shadowing because it affects too 
 // many valid cases. 
 # pragma GCC diagnostic ignored "-Wshadow"

inline std::string tostdu8(const QString& qs)
{
    auto b = qs.toUtf8();
    return std::string(b.begin(), b.end());
}
