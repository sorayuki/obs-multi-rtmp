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
#include <QLineEdit>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QComboBox>
#include <QCheckBox>
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

struct QJsonUtil {
    using JsonIt = QJsonObject::iterator;

    template<class T>
    struct JsonType;

    template<>
    struct JsonType<double> {
        bool Check(JsonIt& it) { return it->isDouble(); }
        double Get(JsonIt& it) { return it->toDouble(); }
    };

    template<>
    struct JsonType<int> {
        bool Check(JsonIt& it) { return it->isDouble(); }
        int Get(JsonIt& it) { return static_cast<int>(std::round(it->toDouble())); }
    };

    template<>
    struct JsonType<QString> {
        bool Check(JsonIt& it) { return it->isString(); }
        QString Get(JsonIt& it) { return it->toString(); }
    };

    template<>
    struct JsonType<std::string> {
        bool Check(JsonIt& it) { return it->isString(); }
        std::string Get(JsonIt& it) { return tostdu8(it->toString()); }
    };

    template<>
    struct JsonType<bool> {
        bool Check(JsonIt& it) { return it->isBool(); }
        bool Get(JsonIt& it) { return it->toBool(); }
    };

    template<class Val>
    static Val Get(QJsonObject& json, const char* key, Val def) {
        auto it = json.find(key);
        if (it != json.end() && JsonType<Val>().Check(it))
            return JsonType<Val>().Get(it);
        else
            return def;
    }

    template<class Val>
    static std::optional<Val> Get(QJsonObject& json, const char* key) {
        auto it = json.find(key);
        if (it != json.end() && JsonType<Val>().Check(it))
            return JsonType<Val>().Get(it);
        else
            return std::optional<Val>{};
    }

    template<class Fun>
    static bool IfGet(QJsonObject& json, const char* key, const Fun& f) {
        using Val = decltype(f({}));
        auto it = json.find(key);
        if (it != json.end() && JsonType<Val>().Check(it)) {
            f(JsonType<Val>().Get(it));
            return true;
        } else {
            return false;
        }
    }
};