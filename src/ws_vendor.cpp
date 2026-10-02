#include "ws_vendor.hpp"
#include "push-widget.h"
#include "multi-output-widget.h"
#include "output-config.h"
#include "plugin-support.h"
#include <obs-websocket-api.h>
#include <QMainWindow>
#include <QDockWidget>
#include <QThread>
#include <QMetaObject>
#include <QAction>
#include <QTimer>
#include <QRegularExpression>

#ifdef _WIN32
#include <Windows.h>
#endif

MultiRTMPWebsocketVendor* MultiRTMPWebsocketVendor::s_instance = nullptr;

MultiRTMPWebsocketVendor* MultiRTMPWebsocketVendor::Instance() {
    if (!s_instance) {
        s_instance = new MultiRTMPWebsocketVendor();
    }
    return s_instance;
}

bool MultiRTMPWebsocketVendor::Initialize() {
#ifdef ENABLE_WEBSOCKET
    blog(LOG_INFO, TAG "Websocket support enabled, attempting to register vendor");
    
    // First ensure the proc handler is available
    if (!obs_websocket_ensure_ph()) {
        blog(LOG_WARNING, TAG "obs-websocket proc handler not available yet, will retry");
        QTimer::singleShot(1000, []() {
            MultiRTMPWebsocketVendor::Instance()->Initialize();
        });
        return false;
    }
    
    // Register the vendor
    obs_websocket_vendor vendor_handle = obs_websocket_register_vendor(m_vendorName.c_str());
    if (!vendor_handle) {
        blog(LOG_ERROR, TAG "Failed to register vendor: %s", m_vendorName.c_str());
        return false;
    }
    
    m_vendorHandle = vendor_handle;
    blog(LOG_INFO, TAG "Vendor registered successfully: %s", m_vendorName.c_str());
    
    // Register all request handlers
    RegisterVendorRequests(vendor_handle);
    return true;
#else
    blog(LOG_INFO, TAG "Websocket support disabled at compile time");
    return false;
#endif
}

void MultiRTMPWebsocketVendor::Shutdown() {
#ifdef ENABLE_WEBSOCKET
    if (m_vendorHandle) {
        m_vendorHandle = nullptr;
        blog(LOG_INFO, TAG "Vendor unregistered: %s", m_vendorName.c_str());
    }
#endif
}

void MultiRTMPWebsocketVendor::RegisterVendorRequests(obs_websocket_vendor vendor_handle) {
#ifdef ENABLE_WEBSOCKET
    auto reg = [vendor_handle](const char* name, obs_websocket_request_callback_function cb) {
        if (!obs_websocket_vendor_register_request(vendor_handle, name, cb, nullptr)) {
            blog(LOG_ERROR, TAG "Failed to register vendor request: %s", name);
        } else {
            blog(LOG_DEBUG, TAG "Registered vendor request: %s", name);
        }
    };

    reg("list_targets", HandleListTargetsRequest);
    reg("get_target_state", HandleGetTargetStateRequest);
    reg("start_target", HandleStartTargetRequest);
    reg("stop_target", HandleStopTargetRequest);
    reg("toggle_target", HandleToggleTargetRequest);
    reg("start_all", HandleStartAllRequest);
    reg("stop_all", HandleStopAllRequest);
    reg("add_target", HandleAddTargetRequest);
    reg("clone_target", HandleCloneTargetRequest);
    reg("update_target_name", HandleUpdateTargetNameRequest);
    reg("update_stream_key", HandleUpdateStreamKeyRequest);
    reg("update_service_param", HandleUpdateServiceParamRequest);
    reg("delete_target", HandleDeleteTargetRequest);
    reg("update_sync_start", HandleUpdateSyncStartRequest);
    reg("update_sync_stop", HandleUpdateSyncStopRequest);
    reg("get_target_stats", HandleGetTargetStatsRequest);

    blog(LOG_INFO, TAG "All 16 vendor requests registered successfully");
#endif
}

MultiOutputWidget* MultiRTMPWebsocketVendor::GetMultiOutputWidget() {
    auto mainwin = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    if (!mainwin) return nullptr;

    auto dock = mainwin->findChild<QDockWidget*>("obs-multi-rtmp-dock");
    if (dock) {
        auto widget = qobject_cast<MultiOutputWidget*>(dock->widget());
        if (widget) return widget;
        auto direct = qobject_cast<MultiOutputWidget*>(dock);
        if (direct) return direct;
    }
    return mainwin->findChild<MultiOutputWidget*>("obs-multi-rtmp-dock");
}

PushWidget* MultiRTMPWebsocketVendor::FindPushWidgetByIdOrName(obs_data_t* request_data) {
    const char* targetId = obs_data_get_string(request_data, "id");
    const char* targetName = obs_data_get_string(request_data, "name");
    
    if ((!targetId || strlen(targetId) == 0) && (!targetName || strlen(targetName) == 0)) {
        return nullptr;
    }
    
    auto multiOutputWidget = GetMultiOutputWidget();
    if (!multiOutputWidget) return nullptr;

    if (targetId && strlen(targetId) > 0) {
        return multiOutputWidget->FindPushWidgetById(QString::fromUtf8(targetId));
    }
    
    if (targetName && strlen(targetName) > 0) {
        QString qTargetName = QString::fromUtf8(targetName);
        for (auto* widget : multiOutputWidget->GetAllPushWidgets()) {
            if (widget && widget->GetTargetName() == qTargetName) {
                return widget;
            }
        }
    }
    return nullptr;
}

void MultiRTMPWebsocketVendor::ExecuteInUIThread(std::function<void()> task) {
    auto mainwin = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    if (mainwin) {
        QMetaObject::invokeMethod(mainwin, [task = std::move(task)]() {
            task();
        }, Qt::QueuedConnection);
    }
}

// -------------------------------------------------------------
// Request Callbacks (C API entry points)
// -------------------------------------------------------------

void MultiRTMPWebsocketVendor::HandleListTargetsRequest(obs_data_t* request_data, obs_data_t* response_data, void* private_data) {
    Q_UNUSED(request_data);
    Q_UNUSED(private_data);
    Instance()->HandleListTargets(response_data);
}

void MultiRTMPWebsocketVendor::HandleGetTargetStateRequest(obs_data_t* request_data, obs_data_t* response_data, void* private_data) {
    Q_UNUSED(private_data);
    Instance()->HandleGetTargetState(request_data, response_data);
}

void MultiRTMPWebsocketVendor::HandleStartTargetRequest(obs_data_t* request_data, obs_data_t* response_data, void* private_data) {
    Q_UNUSED(private_data);
    Instance()->HandleStartTarget(request_data, response_data);
}

void MultiRTMPWebsocketVendor::HandleStopTargetRequest(obs_data_t* request_data, obs_data_t* response_data, void* private_data) {
    Q_UNUSED(private_data);
    Instance()->HandleStopTarget(request_data, response_data);
}

void MultiRTMPWebsocketVendor::HandleToggleTargetRequest(obs_data_t* request_data, obs_data_t* response_data, void* private_data) {
    Q_UNUSED(private_data);
    Instance()->HandleToggleTarget(request_data, response_data);
}

void MultiRTMPWebsocketVendor::HandleStartAllRequest(obs_data_t* request_data, obs_data_t* response_data, void* private_data) {
    Q_UNUSED(request_data);
    Q_UNUSED(private_data);
    Instance()->HandleStartAll(response_data);
}

void MultiRTMPWebsocketVendor::HandleStopAllRequest(obs_data_t* request_data, obs_data_t* response_data, void* private_data) {
    Q_UNUSED(request_data);
    Q_UNUSED(private_data);
    Instance()->HandleStopAll(response_data);
}

void MultiRTMPWebsocketVendor::HandleAddTargetRequest(obs_data_t* request_data, obs_data_t* response_data, void* private_data) {
    Q_UNUSED(private_data);
    Instance()->HandleAddTarget(request_data, response_data);
}

void MultiRTMPWebsocketVendor::HandleCloneTargetRequest(obs_data_t* request_data, obs_data_t* response_data, void* private_data) {
    Q_UNUSED(private_data);
    Instance()->HandleCloneTarget(request_data, response_data);
}

void MultiRTMPWebsocketVendor::HandleUpdateTargetNameRequest(obs_data_t* request_data, obs_data_t* response_data, void* private_data) {
    Q_UNUSED(private_data);
    Instance()->HandleUpdateTargetName(request_data, response_data);
}

void MultiRTMPWebsocketVendor::HandleUpdateStreamKeyRequest(obs_data_t* request_data, obs_data_t* response_data, void* private_data) {
    Q_UNUSED(private_data);
    Instance()->HandleUpdateStreamKey(request_data, response_data);
}

void MultiRTMPWebsocketVendor::HandleUpdateServiceParamRequest(obs_data_t* request_data, obs_data_t* response_data, void* private_data) {
    Q_UNUSED(private_data);
    Instance()->HandleUpdateServiceParam(request_data, response_data);
}

void MultiRTMPWebsocketVendor::HandleDeleteTargetRequest(obs_data_t* request_data, obs_data_t* response_data, void* private_data) {
    Q_UNUSED(private_data);
    Instance()->HandleDeleteTarget(request_data, response_data);
}

void MultiRTMPWebsocketVendor::HandleUpdateSyncStartRequest(obs_data_t* request_data, obs_data_t* response_data, void* private_data) {
    Q_UNUSED(private_data);
    Instance()->HandleUpdateSyncStart(request_data, response_data);
}

void MultiRTMPWebsocketVendor::HandleUpdateSyncStopRequest(obs_data_t* request_data, obs_data_t* response_data, void* private_data) {
    Q_UNUSED(private_data);
    Instance()->HandleUpdateSyncStop(request_data, response_data);
}

void MultiRTMPWebsocketVendor::HandleGetTargetStatsRequest(obs_data_t* request_data, obs_data_t* response_data, void* private_data) {
    Q_UNUSED(private_data);
    Instance()->HandleGetTargetStats(request_data, response_data);
}

// -------------------------------------------------------------
// Internal Handlers
// -------------------------------------------------------------

bool MultiRTMPWebsocketVendor::HandleListTargets(obs_data_t* response_data) {
    auto multiOutputWidget = GetMultiOutputWidget();
    if (!multiOutputWidget) {
        obs_data_set_string(response_data, "error", "Multi RTMP dock not found");
        return false;
    }

    obs_data_array_t* targetsArray = obs_data_array_create();
    auto& global = GlobalMultiOutputConfig();
    int activeCount = 0;

    for (const auto& targetConfig : global.targets) {
        if (!targetConfig) continue;

        obs_data_t* targetData = obs_data_create();
        obs_data_set_string(targetData, "id", targetConfig->id.c_str());
        obs_data_set_string(targetData, "name", targetConfig->name.c_str());
        obs_data_set_bool(targetData, "enabled", true);
        obs_data_set_bool(targetData, "syncStart", targetConfig->syncStart);
        obs_data_set_bool(targetData, "syncStop", targetConfig->syncStop);
        obs_data_set_string(targetData, "protocol", targetConfig->protocol.c_str());

        // Extract server URL from serviceParam JSON if available
        if (targetConfig->serviceParam.contains("server") && targetConfig->serviceParam["server"].is_string()) {
            obs_data_set_string(targetData, "server", targetConfig->serviceParam["server"].get<std::string>().c_str());
        }

        // Check running state & stats from active push widget
        auto widget = multiOutputWidget->FindPushWidgetById(QString::fromStdString(targetConfig->id));
        if (widget) {
            bool running = widget->IsRunning();
            obs_data_set_bool(targetData, "isRunning", running);
            if (running) activeCount++;

            QString status = widget->GetStatusText();
            obs_data_set_string(targetData, "status", status.toUtf8().constData());
            ParseStatusText(status, targetData);
        } else {
            obs_data_set_bool(targetData, "isRunning", false);
            obs_data_set_string(targetData, "status", "stopped");
        }

        obs_data_array_push_back(targetsArray, targetData);
        obs_data_release(targetData);
    }

    obs_data_set_array(response_data, "targets", targetsArray);
    obs_data_set_int(response_data, "count", static_cast<long long>(global.targets.size()));
    obs_data_set_int(response_data, "activeCount", activeCount);
    obs_data_array_release(targetsArray);
    return true;
}

bool MultiRTMPWebsocketVendor::HandleGetTargetState(obs_data_t* request_data, obs_data_t* response_data) {
    PushWidget* targetWidget = FindPushWidgetByIdOrName(request_data);
    if (!targetWidget) {
        obs_data_set_string(response_data, "error", "Target not found");
        return false;
    }

    auto config = targetWidget->GetConfig();
    obs_data_set_string(response_data, "id", targetWidget->GetTargetId().toUtf8().constData());
    obs_data_set_string(response_data, "name", targetWidget->GetTargetName().toUtf8().constData());
    obs_data_set_bool(response_data, "isRunning", targetWidget->IsRunning());
    obs_data_set_string(response_data, "status", targetWidget->GetStatusText().toUtf8().constData());

    if (config) {
        obs_data_set_bool(response_data, "syncStart", config->syncStart);
        obs_data_set_bool(response_data, "syncStop", config->syncStop);
        obs_data_set_string(response_data, "protocol", config->protocol.c_str());
    }
    return true;
}

bool MultiRTMPWebsocketVendor::HandleStartTarget(obs_data_t* request_data, obs_data_t* response_data) {
    PushWidget* targetWidget = FindPushWidgetByIdOrName(request_data);
    if (!targetWidget) {
        obs_data_set_string(response_data, "error", "Target not found");
        return false;
    }

    ExecuteInUIThread([targetWidget]() {
        targetWidget->StartStreaming();
    });

    obs_data_set_string(response_data, "status", "start_requested");
    obs_data_set_string(response_data, "id", targetWidget->GetTargetId().toUtf8().constData());
    return true;
}

bool MultiRTMPWebsocketVendor::HandleStopTarget(obs_data_t* request_data, obs_data_t* response_data) {
    PushWidget* targetWidget = FindPushWidgetByIdOrName(request_data);
    if (!targetWidget) {
        obs_data_set_string(response_data, "error", "Target not found");
        return false;
    }

    ExecuteInUIThread([targetWidget]() {
        targetWidget->StopStreaming();
    });

    obs_data_set_string(response_data, "status", "stop_requested");
    obs_data_set_string(response_data, "id", targetWidget->GetTargetId().toUtf8().constData());
    return true;
}

bool MultiRTMPWebsocketVendor::HandleToggleTarget(obs_data_t* request_data, obs_data_t* response_data) {
    PushWidget* targetWidget = FindPushWidgetByIdOrName(request_data);
    if (!targetWidget) {
        obs_data_set_string(response_data, "error", "Target not found");
        return false;
    }

    bool isRunning = targetWidget->IsRunning();
    ExecuteInUIThread([targetWidget, isRunning]() {
        if (isRunning) {
            targetWidget->StopStreaming();
        } else {
            targetWidget->StartStreaming();
        }
    });

    obs_data_set_string(response_data, "status", isRunning ? "stop_requested" : "start_requested");
    obs_data_set_string(response_data, "id", targetWidget->GetTargetId().toUtf8().constData());
    return true;
}

bool MultiRTMPWebsocketVendor::HandleStartAll(obs_data_t* response_data) {
    auto multiOutputWidget = GetMultiOutputWidget();
    if (!multiOutputWidget) {
        obs_data_set_string(response_data, "error", "Multi RTMP dock not found");
        return false;
    }

    auto widgets = multiOutputWidget->GetAllPushWidgets();
    ExecuteInUIThread([widgets]() {
        for (auto* widget : widgets) {
            if (widget && !widget->IsRunning()) {
                widget->StartStreaming();
            }
        }
    });

    obs_data_set_string(response_data, "status", "start_all_requested");
    obs_data_set_int(response_data, "count", static_cast<long long>(widgets.size()));
    return true;
}

bool MultiRTMPWebsocketVendor::HandleStopAll(obs_data_t* response_data) {
    auto multiOutputWidget = GetMultiOutputWidget();
    if (!multiOutputWidget) {
        obs_data_set_string(response_data, "error", "Multi RTMP dock not found");
        return false;
    }

    auto widgets = multiOutputWidget->GetAllPushWidgets();
    ExecuteInUIThread([widgets]() {
        for (auto* widget : widgets) {
            if (widget && widget->IsRunning()) {
                widget->StopStreaming();
            }
        }
    });

    obs_data_set_string(response_data, "status", "stop_all_requested");
    obs_data_set_int(response_data, "count", static_cast<long long>(widgets.size()));
    return true;
}

bool MultiRTMPWebsocketVendor::HandleAddTarget(obs_data_t* request_data, obs_data_t* response_data) {
    const char* name = obs_data_get_string(request_data, "name");
    const char* protocol = obs_data_get_string(request_data, "protocol");

    if (!name || strlen(name) == 0) {
        obs_data_set_string(response_data, "error", "Name parameter is required");
        return false;
    }

    auto multiOutputWidget = GetMultiOutputWidget();
    if (!multiOutputWidget) {
        obs_data_set_string(response_data, "error", "Multi RTMP dock not found");
        return false;
    }

    QString qName = QString::fromUtf8(name);
    QString qProtocol = protocol ? QString::fromUtf8(protocol) : "RTMP";

    bool success = false;
    auto mainwin = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    if (mainwin && QThread::currentThread() != mainwin->thread()) {
        QMetaObject::invokeMethod(mainwin, [&]() {
            success = multiOutputWidget->AddNewTarget(qName, qProtocol);
        }, Qt::BlockingQueuedConnection);
    } else {
        success = multiOutputWidget->AddNewTarget(qName, qProtocol);
    }

    if (success) {
        obs_data_set_string(response_data, "status", "target_added");
        obs_data_set_string(response_data, "name", name);
    } else {
        obs_data_set_string(response_data, "error", "Failed to add target");
    }
    return success;
}

bool MultiRTMPWebsocketVendor::HandleCloneTarget(obs_data_t* request_data, obs_data_t* response_data) {
    const char* sourceId = obs_data_get_string(request_data, "sourceId");
    const char* newName = obs_data_get_string(request_data, "newName");
    const char* newStreamKey = obs_data_get_string(request_data, "newStreamKey");

    if (!sourceId || strlen(sourceId) == 0) {
        obs_data_set_string(response_data, "error", "sourceId parameter is required");
        return false;
    }

    auto multiOutputWidget = GetMultiOutputWidget();
    if (!multiOutputWidget) {
        obs_data_set_string(response_data, "error", "Multi RTMP dock not found");
        return false;
    }

    QString qSourceId = QString::fromUtf8(sourceId);
    QString qNewName = newName ? QString::fromUtf8(newName) : "";
    QString qKey = newStreamKey ? QString::fromUtf8(newStreamKey) : "";

    bool success = false;
    auto mainwin = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    if (mainwin && QThread::currentThread() != mainwin->thread()) {
        QMetaObject::invokeMethod(mainwin, [&]() {
            success = multiOutputWidget->CloneTarget(qSourceId, qNewName, qKey);
        }, Qt::BlockingQueuedConnection);
    } else {
        success = multiOutputWidget->CloneTarget(qSourceId, qNewName, qKey);
    }

    if (success) {
        obs_data_set_string(response_data, "status", "target_cloned");
    } else {
        obs_data_set_string(response_data, "error", "Failed to clone target");
    }
    return success;
}

bool MultiRTMPWebsocketVendor::HandleUpdateTargetName(obs_data_t* request_data, obs_data_t* response_data) {
    const char* targetId = obs_data_get_string(request_data, "id");
    const char* newName = obs_data_get_string(request_data, "newName");

    if (!targetId || !newName || strlen(targetId) == 0 || strlen(newName) == 0) {
        obs_data_set_string(response_data, "error", "id and newName parameters are required");
        return false;
    }

    auto multiOutputWidget = GetMultiOutputWidget();
    if (!multiOutputWidget) {
        obs_data_set_string(response_data, "error", "Multi RTMP dock not found");
        return false;
    }

    QString qId = QString::fromUtf8(targetId);
    QString qName = QString::fromUtf8(newName);

    bool success = false;
    auto mainwin = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    if (mainwin && QThread::currentThread() != mainwin->thread()) {
        QMetaObject::invokeMethod(mainwin, [&]() {
            success = multiOutputWidget->UpdateTargetName(qId, qName);
        }, Qt::BlockingQueuedConnection);
    } else {
        success = multiOutputWidget->UpdateTargetName(qId, qName);
    }

    if (success) {
        obs_data_set_string(response_data, "status", "name_updated");
    } else {
        obs_data_set_string(response_data, "error", "Failed to update target name");
    }
    return success;
}

bool MultiRTMPWebsocketVendor::HandleUpdateStreamKey(obs_data_t* request_data, obs_data_t* response_data) {
    const char* targetId = obs_data_get_string(request_data, "id");
    const char* streamKey = obs_data_get_string(request_data, "streamKey");

    if (!targetId || !streamKey || strlen(targetId) == 0 || strlen(streamKey) == 0) {
        obs_data_set_string(response_data, "error", "id and streamKey parameters are required");
        return false;
    }

    auto multiOutputWidget = GetMultiOutputWidget();
    if (!multiOutputWidget) {
        obs_data_set_string(response_data, "error", "Multi RTMP dock not found");
        return false;
    }

    QString qId = QString::fromUtf8(targetId);
    QString qKey = QString::fromUtf8(streamKey);

    bool success = false;
    auto mainwin = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    if (mainwin && QThread::currentThread() != mainwin->thread()) {
        QMetaObject::invokeMethod(mainwin, [&]() {
            success = multiOutputWidget->UpdateTargetStreamKey(qId, qKey);
        }, Qt::BlockingQueuedConnection);
    } else {
        success = multiOutputWidget->UpdateTargetStreamKey(qId, qKey);
    }

    if (success) {
        obs_data_set_string(response_data, "status", "stream_key_updated");
    } else {
        obs_data_set_string(response_data, "error", "Failed to update stream key");
    }
    return success;
}

bool MultiRTMPWebsocketVendor::HandleUpdateServiceParam(obs_data_t* request_data, obs_data_t* response_data) {
    const char* targetId = obs_data_get_string(request_data, "id");
    const char* key = obs_data_get_string(request_data, "key");
    const char* value = obs_data_get_string(request_data, "value");

    if (!targetId || !key || !value) {
        obs_data_set_string(response_data, "error", "id, key, and value parameters are required");
        return false;
    }

    auto multiOutputWidget = GetMultiOutputWidget();
    if (!multiOutputWidget) {
        obs_data_set_string(response_data, "error", "Multi RTMP dock not found");
        return false;
    }

    QString qId = QString::fromUtf8(targetId);
    QString qKey = QString::fromUtf8(key);
    QString qVal = QString::fromUtf8(value);

    bool success = false;
    auto mainwin = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    if (mainwin && QThread::currentThread() != mainwin->thread()) {
        QMetaObject::invokeMethod(mainwin, [&]() {
            success = multiOutputWidget->UpdateTargetServiceParam(qId, qKey, qVal);
        }, Qt::BlockingQueuedConnection);
    } else {
        success = multiOutputWidget->UpdateTargetServiceParam(qId, qKey, qVal);
    }

    if (success) {
        obs_data_set_string(response_data, "status", "service_param_updated");
    } else {
        obs_data_set_string(response_data, "error", "Failed to update service parameter");
    }
    return success;
}

bool MultiRTMPWebsocketVendor::HandleDeleteTarget(obs_data_t* request_data, obs_data_t* response_data) {
    const char* targetId = obs_data_get_string(request_data, "id");

    if (!targetId || strlen(targetId) == 0) {
        obs_data_set_string(response_data, "error", "id parameter is required");
        return false;
    }

    auto multiOutputWidget = GetMultiOutputWidget();
    if (!multiOutputWidget) {
        obs_data_set_string(response_data, "error", "Multi RTMP dock not found");
        return false;
    }

    QString qId = QString::fromUtf8(targetId);
    auto pushWidget = multiOutputWidget->FindPushWidgetById(qId);
    if (!pushWidget) {
        obs_data_set_string(response_data, "error", "Target not found");
        return false;
    }

    if (pushWidget->IsEditing()) {
        obs_data_set_string(response_data, "error", "Target is currently being edited in OBS UI");
        return false;
    }

    bool success = false;
    auto mainwin = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    if (mainwin && QThread::currentThread() != mainwin->thread()) {
        QMetaObject::invokeMethod(mainwin, [&]() {
            success = multiOutputWidget->DeleteTarget(qId);
        }, Qt::BlockingQueuedConnection);
    } else {
        success = multiOutputWidget->DeleteTarget(qId);
    }

    if (success) {
        obs_data_set_string(response_data, "status", "target_deleted");
    } else {
        obs_data_set_string(response_data, "error", "Failed to delete target");
    }
    return success;
}

bool MultiRTMPWebsocketVendor::HandleUpdateSyncStart(obs_data_t* request_data, obs_data_t* response_data) {
    const char* targetId = obs_data_get_string(request_data, "id");
    bool syncStart = obs_data_get_bool(request_data, "syncStart");

    if (!targetId || strlen(targetId) == 0) {
        obs_data_set_string(response_data, "error", "id parameter is required");
        return false;
    }

    auto multiOutputWidget = GetMultiOutputWidget();
    if (!multiOutputWidget) {
        obs_data_set_string(response_data, "error", "Multi RTMP dock not found");
        return false;
    }

    QString qId = QString::fromUtf8(targetId);
    bool success = false;
    auto mainwin = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    if (mainwin && QThread::currentThread() != mainwin->thread()) {
        QMetaObject::invokeMethod(mainwin, [&]() {
            success = multiOutputWidget->UpdateSyncStart(qId, syncStart);
        }, Qt::BlockingQueuedConnection);
    } else {
        success = multiOutputWidget->UpdateSyncStart(qId, syncStart);
    }

    if (success) {
        obs_data_set_string(response_data, "status", "sync_start_updated");
    } else {
        obs_data_set_string(response_data, "error", "Failed to update sync start");
    }
    return success;
}

bool MultiRTMPWebsocketVendor::HandleUpdateSyncStop(obs_data_t* request_data, obs_data_t* response_data) {
    const char* targetId = obs_data_get_string(request_data, "id");
    bool syncStop = obs_data_get_bool(request_data, "syncStop");

    if (!targetId || strlen(targetId) == 0) {
        obs_data_set_string(response_data, "error", "id parameter is required");
        return false;
    }

    auto multiOutputWidget = GetMultiOutputWidget();
    if (!multiOutputWidget) {
        obs_data_set_string(response_data, "error", "Multi RTMP dock not found");
        return false;
    }

    QString qId = QString::fromUtf8(targetId);
    bool success = false;
    auto mainwin = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    if (mainwin && QThread::currentThread() != mainwin->thread()) {
        QMetaObject::invokeMethod(mainwin, [&]() {
            success = multiOutputWidget->UpdateSyncStop(qId, syncStop);
        }, Qt::BlockingQueuedConnection);
    } else {
        success = multiOutputWidget->UpdateSyncStop(qId, syncStop);
    }

    if (success) {
        obs_data_set_string(response_data, "status", "sync_stop_updated");
    } else {
        obs_data_set_string(response_data, "error", "Failed to update sync stop");
    }
    return success;
}

bool MultiRTMPWebsocketVendor::HandleGetTargetStats(obs_data_t* request_data, obs_data_t* response_data) {
    PushWidget* targetWidget = FindPushWidgetByIdOrName(request_data);
    if (!targetWidget) {
        obs_data_set_string(response_data, "error", "Target not found");
        return false;
    }

    QString statusText;
    auto mainwin = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    if (mainwin && QThread::currentThread() != mainwin->thread()) {
        QMetaObject::invokeMethod(targetWidget, [targetWidget, &statusText]() {
            statusText = targetWidget->GetStatusText();
        }, Qt::BlockingQueuedConnection);
    } else {
        statusText = targetWidget->GetStatusText();
    }

    ParseStatusText(statusText, response_data);

    obs_data_set_string(response_data, "id", targetWidget->GetTargetId().toUtf8().constData());
    obs_data_set_string(response_data, "name", targetWidget->GetTargetName().toUtf8().constData());
    obs_data_set_bool(response_data, "isRunning", targetWidget->IsRunning());
    obs_data_set_string(response_data, "rawStatus", statusText.toUtf8().constData());
    return true;
}

void MultiRTMPWebsocketVendor::ParseStatusText(const QString& statusText, obs_data_t* response_data) {
    if (statusText.isEmpty()) {
        obs_data_set_string(response_data, "duration", "00:00:00");
        obs_data_set_string(response_data, "bitrate", "0 bps");
        obs_data_set_string(response_data, "fps", "0 FPS");
        obs_data_set_double(response_data, "bitrateValue", 0);
        obs_data_set_double(response_data, "fpsValue", 0);
        return;
    }

    // Split on whitespace or double-spaces
    QStringList parts = statusText.split(QRegularExpression("\\s{2,}"), Qt::SkipEmptyParts);

    if (parts.size() >= 3) {
        obs_data_set_string(response_data, "duration", parts[0].toUtf8().constData());

        QString bitrateStr = parts[1];
        obs_data_set_string(response_data, "bitrate", bitrateStr.toUtf8().constData());
        obs_data_set_double(response_data, "bitrateValue", ParseBitrateValue(bitrateStr));

        QString fpsStr = parts[2];
        obs_data_set_string(response_data, "fps", fpsStr.toUtf8().constData());
        obs_data_set_double(response_data, "fpsValue", ParseFpsValue(fpsStr));
    } else {
        obs_data_set_string(response_data, "duration", "00:00:00");
        obs_data_set_string(response_data, "bitrate", "0 bps");
        obs_data_set_string(response_data, "fps", "0 FPS");
        obs_data_set_double(response_data, "bitrateValue", 0);
        obs_data_set_double(response_data, "fpsValue", 0);
    }
}

double MultiRTMPWebsocketVendor::ParseBitrateValue(const QString& bitrateStr) {
    QRegularExpression re("([0-9.]+)\\s*(kbps|Mbps|bps)", QRegularExpression::CaseInsensitiveOption);
    auto match = re.match(bitrateStr);
    if (!match.hasMatch()) return 0.0;

    double val = match.captured(1).toDouble();
    QString unit = match.captured(2).toLower();
    if (unit == "mbps") return val * 1000000.0;
    if (unit == "kbps") return val * 1000.0;
    return val;
}

double MultiRTMPWebsocketVendor::ParseFpsValue(const QString& fpsStr) {
    QRegularExpression re("([0-9.]+)\\s*fps", QRegularExpression::CaseInsensitiveOption);
    auto match = re.match(fpsStr);
    if (!match.hasMatch()) return 0.0;
    return match.captured(1).toDouble();
}
