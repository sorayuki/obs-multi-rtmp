#include "pch.h"

#include <list>
#include <regex>
#include <filesystem>

#include "push-widget.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#define ConfigSection "obs-multi-rtmp"

static class GlobalServiceImpl : public GlobalService
{
public:
    void SaveConfig() override {
        if (saveConfig_) {
            saveConfig_();
        }
    }

    bool RunInUIThread(std::function<void()> task) override {
        if (uiThread_ == nullptr)
            return false;
        QMetaObject::invokeMethod(uiThread_, [func = std::move(task)]() {
            func();
        });
        return true;
    }

    QThread* uiThread_ = nullptr;
    std::function<void()> saveConfig_;
} s_service;


GlobalService& GetGlobalService() {
    return s_service;
}


class MultiOutputWidget : public QDockWidget
{
    int dockLocation_;
    bool dockVisible_;
    bool reopenShown_;

public:
    MultiOutputWidget(QWidget* parent = 0)
        : QDockWidget(parent)
        , reopenShown_(false)
    {
        setWindowTitle(obs_module_text("Title"));
        setFeatures((DockWidgetFeatures)(AllDockWidgetFeatures & ~DockWidgetClosable));

        // save dock location
        QObject::connect(this, &QDockWidget::dockLocationChanged, [this](Qt::DockWidgetArea area) {
            dockLocation_ = area;
        });

        scroll_ = new QScrollArea(this);
        scroll_->move(0, 22);

        container_ = new QWidget(this);
        layout_ = new QGridLayout(container_);
        layout_->setAlignment(Qt::AlignmentFlag::AlignTop);

        // init widget
        addButton_ = new QPushButton(obs_module_text("Btn.NewTarget"), container_);
        QObject::connect(addButton_, &QPushButton::clicked, [this]() {
            auto pushwidget = createPushWidget(QJsonObject(), container_);
            layout_->addWidget(pushwidget);
            if (pushwidget->ShowEditDlg())
                GetGlobalService().SaveConfig();
            else
                delete pushwidget;
        });
        layout_->addWidget(addButton_);

        if (std::string("\xe5\xa4\x9a\xe8\xb7\xaf\xe6\x8e\xa8\xe6\xb5\x81") == obs_module_text("Title"))
            layout_->addWidget(new QLabel(u8"本插件免费提供，请不要为此付费。\n作者：雷鸣", container_));
        else
            layout_->addWidget(new QLabel(u8"This plugin provided free of charge. \nAuthor: SoraYuki", container_));

        // load config
        LoadConfig();

        scroll_->setWidgetResizable(true);
        scroll_->setWidget(container_);

        setLayout(layout_);

        resize(200, 400);

        s_service.saveConfig_ = [this]() {
            s_service.RunInUIThread([this]() {
                SaveConfig();
            });
        };
    }

    void visibleToggled(bool visible)
    {
        dockVisible_ = visible;
        return;

        if (visible == false
            && reopenShown_ == false
            && !config_has_user_value(obs_frontend_get_global_config(), ConfigSection, "DockVisible"))
        {
            reopenShown_ = true;
            QMessageBox(QMessageBox::Icon::Information, 
                obs_module_text("Notice.Title"), 
                obs_module_text("Notice.Reopen"), 
                QMessageBox::StandardButton::Ok,
                this
            ).exec();
        }
    }

    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::Resize)
        {
            scroll_->resize(width(), height() - 22);
        }
        return QDockWidget::event(event);
    }

    std::vector<PushWidget*> GetAllPushWidgets()
    {
        std::vector<PushWidget*> result;
        for(auto& c : container_->children())
        {
            if (c->objectName() == "push-widget")
            {
                auto w = dynamic_cast<PushWidget*>(c);
                result.push_back(w);
            }
        }
        return result;
    }

    void StopAll()
    {
        for(auto x : GetAllPushWidgets())
            x->Stop();
    }

    void RemoveAll()
    {

    }

    void SaveConfig()
    {
        auto profile_config = obs_frontend_get_profile_config();
        
        QJsonArray targetlist;
        for(auto x : GetAllPushWidgets())
            targetlist.append(x->Config());
        QJsonObject root;
        root["targets"] = targetlist;
        QJsonDocument jsondoc;
        jsondoc.setObject(root);
        config_set_string(profile_config, ConfigSection, "json", jsondoc.toBinaryData().toBase64());

        config_set_int(profile_config, ConfigSection, "DockLocation", (int)dockLocation_);
        config_set_bool(profile_config, ConfigSection, "DockVisible", dockVisible_);

        config_save_safe(profile_config, "tmp", "bak");
    }

    void LoadConfig()
    {
        auto profile_config = obs_frontend_get_profile_config();

        QJsonObject conf;
        auto base64str = config_get_string(profile_config, ConfigSection, "json");
        if (!base64str || !*base64str) { // compatible with old version
            base64str = config_get_string(obs_frontend_get_global_config(), ConfigSection, "json");
        }

        if (base64str && *base64str)
        {
            auto bindat = QByteArray::fromBase64(base64str);
            auto jsondoc = QJsonDocument::fromBinaryData(bindat);
            if (jsondoc.isObject()) {
                conf = jsondoc.object();

                // load succeed. remove all existing widgets
                for (auto x : GetAllPushWidgets())
                    delete x;
            }
        }

        auto targets = conf.find("targets");
        if (targets != conf.end() && targets->isArray())
        {
            for(auto x : targets->toArray())
            {
                if (x.isObject())
                {
                    auto pushwidget = createPushWidget(((QJsonValue)x).toObject(), container_);
                    layout_->addWidget(pushwidget);
                }
            }
        }
    }

private:
    QWidget* container_ = 0;
    QScrollArea* scroll_ = 0;
    QGridLayout* layout_ = 0;
    QPushButton* addButton_ = 0;
};

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-multi-rtmp", "en-US")
OBS_MODULE_AUTHOR("雷鳴 (@sorayukinoyume)")

bool obs_module_load()
{
    // check obs version
    if (obs_get_version() < MAKE_SEMANTIC_VERSION(26, 1, 0))
        return false;
    
    // check old version
#ifdef _WIN32
    {
        std::vector<wchar_t> szExePath(MAX_PATH);
        if (GetModuleFileNameW(NULL, szExePath.data(), szExePath.size()) > 0) {
            auto old_data_dir = std::filesystem::path(szExePath.data())
                .parent_path() // 32bit or 64bit
                .parent_path() // bin
                .parent_path() // install dir
                .append(L"data")
                .append(L"obs-plugins")
                .append(L"obs-multi-rtmp");

            auto old_32bit_file = std::filesystem::path(szExePath.data())
                .parent_path() // 32bit or 64bit
                .parent_path() // bin
                .parent_path() // install dir
                .append(L"obs-plugins")
                .append(L"32bit")
                .append(L"obs-multi-rtmp.dll");

            auto old_64bit_file = std::filesystem::path(szExePath.data())
                .parent_path() // 32bit or 64bit
                .parent_path() // bin
                .parent_path() // install dir
                .append(L"obs-plugins")
                .append(L"64bit")
                .append(L"obs-multi-rtmp.dll");

            std::wstring pathlist;
            if (std::filesystem::exists(old_data_dir)) {
                pathlist += L"\n" + old_data_dir.wstring();
            }

            if (std::filesystem::exists(old_32bit_file)) {
                pathlist += L"\n" + old_32bit_file.wstring();
            }

            if (std::filesystem::exists(old_64bit_file)) {
                pathlist += L"\n" + old_64bit_file.wstring();
            }
            
            if (!pathlist.empty()) {
                MessageBoxW(NULL, 
                    (LR"__(插件加载失败！请先手工删除本插件的旧版本。
ロード失敗。お手順ですがこのプラグインの古いバージョンを削除してください。
Fail to load obs-multi-rtmp. Please remove old versions of this plugin.

旧版残留 / 残りファイル / Old version files:)__" + pathlist).c_str(), 
                    L"OBS-MULTI-RTMP 错误 / エラー / Error", MB_ICONERROR | MB_OK
                );
                return false;
            }
        }
    }
#endif
    
    auto mainwin = (QMainWindow*)obs_frontend_get_main_window();
    if (mainwin == nullptr)
        return false;
    QMetaObject::invokeMethod(mainwin, []() {
        s_service.uiThread_ = QThread::currentThread();
    });

    auto dock = new MultiOutputWidget(mainwin);
    auto action = (QAction*)obs_frontend_add_dock(dock);
    QObject::connect(action, &QAction::toggled, dock, &MultiOutputWidget::visibleToggled);

    // begin work around obs not remember dock geometry added by obs_frontend_add_dock
    mainwin->removeDockWidget(dock);
    auto docklocation = config_get_int(obs_frontend_get_global_config(), ConfigSection, "DockLocation");
    auto visible = config_get_bool(obs_frontend_get_global_config(), ConfigSection, "DockVisible");
    if (!config_has_user_value(obs_frontend_get_global_config(), ConfigSection, "DockLocation"))
    {
        docklocation = Qt::DockWidgetArea::LeftDockWidgetArea;
    }
    if (!config_has_user_value(obs_frontend_get_global_config(), ConfigSection, "DockVisible"))
    {
        visible = true;
    }

    mainwin->addDockWidget((Qt::DockWidgetArea)docklocation, dock);
    if (visible)
    {
        dock->setVisible(true);
        action->setChecked(true);
    }
    else
    {
        dock->setVisible(false);
        action->setChecked(false);
    }
    // end work around

    obs_frontend_add_event_callback(
        [](enum obs_frontend_event event, void *private_data) {
            if (event == obs_frontend_event::OBS_FRONTEND_EVENT_EXIT)
            {
                static_cast<MultiOutputWidget*>(private_data)->StopAll();
            }
            else if (event == obs_frontend_event::OBS_FRONTEND_EVENT_PROFILE_CHANGED)
            {
                static_cast<MultiOutputWidget*>(private_data)->LoadConfig();
            }
        }, dock
    );

    return true;
}

const char *obs_module_description(void)
{
    return "Multiple RTMP Output Plugin";
}
