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
                SaveConfig();
            else
                delete pushwidget;
        });
        layout_->addWidget(addButton_);

        // donate
        if (std::string("\xe5\xa4\x9a\xe8\xb7\xaf\xe6\x8e\xa8\xe6\xb5\x81") == obs_module_text("Title"))
        {
            auto cr = new QWidget(container_);
            auto innerLayout = new QGridLayout(cr);
            innerLayout->setAlignment(Qt::AlignmentFlag::AlignLeft);

            auto label = new QLabel(u8"本插件免费提供，也可以投喂作者。", cr);
            innerLayout->addWidget(label, 0, 0, 1, 2);
            innerLayout->setColumnStretch(0, 4);
            auto label2 = new QLabel(u8"作者：雷鸣", cr);
            innerLayout->addWidget(label2, 1, 0, 1, 1);
            auto btnFeed = new QPushButton(u8"投喂", cr);
            innerLayout->addWidget(btnFeed, 1, 1, 1, 1);
            QObject::connect(btnFeed, &QPushButton::clicked, [this]() {
                const char alipaypng[] = 
                    "iVBORw0KGgoAAAANSUhEUgAAAL4AAAC+CAMAAAC8qkWvAAADAFBMVEX///8AAAAAAAAAAAAAAAAAAAA"
                    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAALI7fhAAAACXBIWXMAAAsSAAALEg"
                    "HS3X78AAAByUlEQVR4nO3QwQ3DMAwEwaT/plOAcMTSch6U9l6BQ5Nz/nyMMcZcmS9OerfeTJ5vGOTLl"
                    "z+Ynwj1wXpy/Z04zwzy5cs/g0/WkyqkxI5Bvnz5t/H5E1Jdvnz58vf5HJsm5cuXfxs/hUDSwZ2iwCBf"
                    "vvzBfJJ1zbu/uUG+fPnT+d2sa3aA25xu5Mt/HPlv8tPKdDCdJf92q4CK8uXLH8mv1z8rWm/uvgWqyJc"
                    "vfzA/jXQLpZByvKJ8+fLn8tNL6WD3FKlObpVfRL58+cP4ZLBbop5MSRfTjHz58ufy0xpS5Rm/vpv2yJ"
                    "cv/yR+d1k9SerW8/VF+fLln8TfSQJ2iybs3+DrevnyW5G/bGvy6/EESdj0Fi9Xb5YvX/6p/DRel0vzO"
                    "9XTXfny5c/lE3gikFOcv1YHn0e+fPkj+WnBDpMc54XqcvLly5/IJ5y0YJ3pTvIN8uXLP4nfDTm4/0n4"
                    "vHz58ifyEye9Sr5FTUs703758uWfyk+Qev36bv2cfADyeeTLl38SP9HI8S5qvUWqy5cv/04+4aTJEtL"
                    "wyJcv/zY+h3RTY+XLl38qv16cjnQ/wDrJr8uXL/8kPkmqQlDd/fUT+fLlT+cbY4y5Ij9rnzvpMKGpCA"
                    "AAAABJRU5ErkJggg==";
                const char wechatpng[] = 
                    "iVBORw0KGgoAAAANSUhEUgAAALQAAAC0CAIAAACyr5FlAAAEJklEQVR4nO3dwW7lKBAF0Mlo/v+XM4v"
                    "srFz0MFVAt87Zpm2T9BUqYSh/fX9//wO/+ff0ALiXcBAJB5FwEAkHkXAQCQfRf+kHX19fe0bwWGgZP7"
                    "dwVWblFxyPeTzIlX/c59dhmDmIhINIOIiEgygWpA/bKsG+Bz3uvFJUPqwUlVPP3VyPmzmIhINIOIiEg"
                    "+jTgvRhauVuqow6tYC48qCpKrJwzH3/Cz/MHETCQSQcRMJB9LIg3aZw4XJ8q+7i7k9k5iASDiLhIBIO"
                    "otsL0oe+lcqp+nTq2qmtAlcxcxAJB5FwEAkH0cuCtG+JsPC80PjOK4epLqkiuxdqzRxEwkEkHETCQfR"
                    "pQbqtBFtZfHzYdm3hmMc2F8JmDiLhIBIOIuEg+rp8O+SfWOv1DXIzMweRcBAJB5FwEMWCtLBN5/jOfe"
                    "/ox1bO/m97UOGv/+LvbOYgEg4i4SASDqKXfUj7WsEXvmefUjjI8Z3Ht5oa1dS1L2pqMweRcBAJB5FwE"
                    "MWCdKX069sHOtb3fr9vkCu6NwOYOYiEg0g4iISD6OUr+77vfZ7aFtrXGH/lVmf/sGYOIuEgEg4i4SCq"
                    "2UNa2GS+7wX3SuPRUz2iznbvN3MQCQeRcBAJB9HLU/anFh8vKTnH1z5sO2VfXq6aOYiEg0g4iISD6NM"
                    "V0r5tkne2Qerr+Fm4s7W7J5aZg0g4iISDSDiIXjbG7zspP37uyoO2/Qo315hTzBxEwkEkHETCQfTpK/"
                    "tt77u39SGdGsbKtXd+dvRBH1LmCAeRcBAJB1HNHtJTZ+G37Q04VSYX0hifSsJBJBxEwkH06Sv7QoULi"
                    "CuFYV871L9gx8IPMweRcBAJB5FwELXsIV25dts65vjah1MnkcbDmGKFlErCQSQcRMJBVHPKfltfzinb"
                    "dqee2gzQ3W7KzEEkHETCQSQcRAca43e/aE7u3Pd66rsAn9zZzEEkHETCQSQcRC+/Zf8wVaCtrJCeerH"
                    "e96Dxncejevzj8pVoMweRcBAJB5FwEL1s+3Sqt+a2A1FjheuYY9s+cvUrMweRcBAJB5FwEL3cQ7rtjN"
                    "Op0u/v6/P04FATS4SDSDiIhIPoZR/SPnd2pL/zfFTfkfwfZg4i4SASDiLhIIp7SE+dhS8s7gpXV0+tg"
                    "W7rmPUrMweRcBAJB5FwEH3ah7Sw2Lmk7dPKAaFtTbDOHsQycxAJB5FwEAkH0csvNV3SprPwp9tKzqnv"
                    "R/VxqIklwkEkHETCQXTg06FjK/tA+w6wF77f72sZNeaUPZWEg0g4iISD6LqCdMrKsaXCdlN9rf7PHvY"
                    "3cxAJB5FwEAkH0cuC9NTZ/MLlxW2fSBorLHXLm2+ZOYiEg0g4iISD6NOC9JJWm31feSq8tu9ofOHOVq"
                    "/sWSIcRMJBJBxE1/Uh5R5mDiLhIBIOIuEgEg4i4SD6HwwNH1Is1PfhAAAAAElFTkSuQmCC";
                auto donateWnd = new QDialog();
                donateWnd->setWindowTitle(u8"投喂");
                auto aliBtn = new QPushButton(u8"支付宝", donateWnd);
                auto aliQr = new QLabel(donateWnd);
                auto aliQrBmp = QPixmap::fromImage(QImage::fromData(QByteArray::fromBase64(QByteArray::fromRawData(alipaypng, sizeof(alipaypng) - 1)), "png"));
                aliQr->setPixmap(aliQrBmp);
                auto weBtn = new QPushButton(u8"微信", donateWnd);
                auto weQr = new QLabel(donateWnd);
                auto weQrBmp = QPixmap::fromImage(QImage::fromData(QByteArray::fromBase64(QByteArray::fromRawData(wechatpng, sizeof(wechatpng) - 1)), "png"));
                weQr->setPixmap(weQrBmp);
                auto layout = new QGridLayout(donateWnd);
                layout->addWidget(aliBtn, 0, 0);
                layout->addWidget(weBtn, 0, 1);
                layout->addWidget(aliQr, 1, 0, 1, 2);
                layout->addWidget(weQr, 1, 0, 1, 2);
                weQr->setVisible(false);
                QObject::connect(aliBtn, &QPushButton::clicked, [aliQr, weQr]() {
                    aliQr->setVisible(true);
                    weQr->setVisible(false);
                });
                QObject::connect(weBtn, &QPushButton::clicked, [aliQr, weQr]() {
                    aliQr->setVisible(false);
                    weQr->setVisible(true);
                });
                donateWnd->setLayout(layout);
                donateWnd->exec();
            });

            layout_->addWidget(cr);
        }
        else
        {
            auto label = new QLabel(u8"<p>This plugin provided free. <br>Author: SoraYuki (<a href=\"https://paypal.me/sorayuki0\">donate</a>) </p>", container_);
            label->setTextFormat(Qt::RichText);
            label->setTextInteractionFlags(Qt::TextBrowserInteraction);
            label->setOpenExternalLinks(true);
            layout_->addWidget(label);
        }


        // load config
        LoadConfig();

        scroll_->setWidgetResizable(true);
        scroll_->setWidget(container_);

        setLayout(layout_);

        resize(200, 400);
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
        config_set_string(profile_config, ConfigSection, "json", jsondoc.toBinaryData().toBase64().constData());

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
    if (false) {
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
    dock->setObjectName("obs-multi-rtmp-dock");
    auto action = (QAction*)obs_frontend_add_dock(dock);
    QObject::connect(action, &QAction::toggled, dock, &MultiOutputWidget::visibleToggled);

    obs_frontend_add_event_callback(
        [](enum obs_frontend_event event, void *private_data) {
            if (event == obs_frontend_event::OBS_FRONTEND_EVENT_EXIT)
            {
                static_cast<MultiOutputWidget*>(private_data)->SaveConfig();
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
