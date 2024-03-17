#include "edit-widget.h"
#include "output-config.h"
#include "json-util.hpp"
#include "obs.hpp"
#include <QMenu>
#include <QTabWidget>
#include <QScrollBar>
#include <qevent.h>
#include <QComboBox>

#include <regex>

#include "obs-properties-widget.h"
#include "protocols.h"

static std::optional<int> ParseStringToInt(const QString& str) {
    try {
        return std::stoi(tostdu8(str));
    }
    catch(...)
    {
        return {};
    }
}


static nlohmann::json to_json(obs_data* data) {
    if (!data)
        return {};
    auto jsonstr = obs_data_get_json(data);
    if (!jsonstr)
        return {};
    try {
        return nlohmann::json::parse(jsonstr);
    }
    catch (...) {
        return {};
    }
}

static OBSData from_json(nlohmann::json j) {
    if (j.type() == nlohmann::json::value_t::null)
        return OBSDataAutoRelease(obs_data_create()).Get();
    
    auto jstr = j.dump();
    OBSData r = obs_data_create_from_json(jstr.c_str());
    if (!r)
        return {};
    obs_data_release(r);
    return r;
}

static obs_properties* AddBF(obs_properties* p) {
    auto bfp = obs_properties_get(p, "bf");
    if (!bfp) {
        bfp = obs_properties_get(p, "bframes");
        if (!bfp)
            obs_properties_add_int(p, "bf", obs_module_text("BFrames"), 0, 16, 1);
    }
    return p;
}


template<class T>
class EventFilter: public QObject {
    T lambda_;
public:
    bool eventFilter(QObject *watched, QEvent *event) override {
        return lambda_(watched, event);
    }
    
    EventFilter(QObject* parent, T&& lambda): lambda_(std::move(lambda)) {
        setParent(parent);
    }
};


class EditOutputWidgetImpl: public EditOutputWidget
{
    QWidget* container_;
    QScrollArea* scroll_;

    std::string targetid_;
    OutputTargetConfigPtr config_ = nullptr;

    QLineEdit* name_ = 0;

    class PropertiesWidget: public QWidget {
    public:
        PropertiesWidget(QWidget* parent)
            : QWidget(parent)
        {
            layout_ = new QGridLayout(this);
            layout_->setRowStretch(0, 1);
            layout_->setColumnStretch(0, 1);
            layout_->setContentsMargins(0, 0, 0, 0);
            setLayout(layout_);
        }

        // owner of props and settings is transfered
        void UpdateProperties(obs_properties* props, obs_data* settings) {
            ResetWid();
            layout_->addWidget(wid_ = createPropertyWidget(props, settings_ = settings, this), 0, 0, 1, 1);
            obs_data_release(settings);
        }

        void ClearProperties() {
            ResetWid();
            layout_->addWidget(new QWidget(this), 0, 0, 1, 1);
        }

        nlohmann::json Save() {
            if (!wid_ || !settings_)
                return {};
            wid_->Save();
            return to_json(settings_);
        }

    private:
        QGridLayout* layout_ = nullptr;
        QPropertiesWidget* wid_ = nullptr;
        OBSData settings_;

        void ResetWid() {
            if (wid_) {
                delete wid_;
                wid_ = nullptr;
            }
        }
    };

    PropertiesWidget* serviceSettings_;
    PropertiesWidget* outputSettings_;
    std::string supported_video_encoders_;
    std::string supported_audio_encoders_;
    PropertiesWidget* videoEncoderSettings_;
    PropertiesWidget* audioEncoderSettings_;
   
    QComboBox* protocolSelector_ = 0;
    QComboBox* venc_ = 0;
    QComboBox* v_scene_ = 0;
    QLineEdit* v_resolution_ = 0;
    QComboBox* v_fpsdenumerator_ = 0;
    QLabel* v_share_notify_ = 0;

    QComboBox* aenc_ = 0;
    QComboBox* a_mixer_ = 0;
    QLabel* a_share_notify_ = 0;

    QCheckBox* syncStart_ = 0;
    QCheckBox *syncStop_ = 0;

    std::vector<std::string> EnumEncodersByCodec(const char* codec)
    {
        if (!codec)
            return {};

        std::vector<std::string> result;
        int i = 0;
        for(;;)
        {
            const char* encid;
            if (!obs_enum_encoder_types(i++, &encid))
                break;
            auto caps = obs_get_encoder_caps(encid);
            if (caps & OBS_ENCODER_CAP_DEPRECATED)
                continue;
            auto enc_codec = obs_get_encoder_codec(encid);
            if (strcmp(enc_codec, codec) == 0)
                result.emplace_back(encid);
        }
        return result;
    }


    template<class T>
    static std::optional<std::string> DuplicateConfig(std::list<T>& list, std::string oldid) {
        auto cfg = FindById(list, oldid);
        if (!cfg) {
            return {};
        }
        auto& global = GlobalMultiOutputConfig();
        auto newid = GenerateId(global);
        auto newcfg = std::make_shared<typename T::element_type>(*cfg);
        newcfg->id = newid;
        list.push_back(newcfg);
        return newid;
    }


    void PopupShareMenu(bool isAudio) {
        auto menu = new QMenu(this);
        auto& global = GlobalMultiOutputConfig();

        {
            auto action = menu->addAction(obs_module_text("NoShare"));
            QObject::connect(action, &QAction::triggered, [=]() {
                auto& global = GlobalMultiOutputConfig();
                if (isAudio) {
                    if (!config_->audioConfig.has_value())
                        return;
                    auto newid = DuplicateConfig(global.audioConfig, *config_->audioConfig);
                    if (!newid.has_value()) {
                        config_->audioConfig.reset();
                        return;
                    }
                    config_->audioConfig = *newid;
                }
                else {
                    if (!config_->videoConfig.has_value())
                        return;
                    auto newid = DuplicateConfig(global.videoConfig, *config_->videoConfig);
                    if (!newid.has_value()) {
                        config_->videoConfig.reset();
                        return;
                    }
                    config_->videoConfig = *newid;
                }
            });
        }

        std::unordered_map<std::string, std::string> items;
        for(auto& target: global.targets) {
            if (isAudio) {
                if (target->audioConfig.has_value()) {
                    auto id = *target->audioConfig;
                    auto it = items.find(id);
                    if (it != items.end()) {
                        it->second += ", " + target->name;
                    }
                    else {
                        items.insert(std::make_pair(id, target->name));
                    }
                }
            } else {
                if (target->videoConfig.has_value()) {
                    auto id = *target->videoConfig;
                    auto it = items.find(id);
                    if (it != items.end()) {
                        it->second += ", " + target->name;
                    }
                    else {
                        items.insert(std::make_pair(id, target->name));
                    }
                }
            }
        }

        for (auto& item : items) {
            auto action = menu->addAction(QString::fromUtf8(item.second));
            QObject::connect(action, &QAction::triggered, [=]() {
                if (isAudio)
                    config_->audioConfig = item.first;
                else
                    config_->videoConfig = item.first;
            });
        }

        menu->exec(QCursor::pos());
    }

    QTabWidget* CreateOutputSettingsWidget(QWidget* parent) {
        auto tab = new QTabWidget(parent);

        // service
        {
            serviceSettings_ = new PropertiesWidget(tab);
            updateServiceTab();
            tab->addTab(serviceSettings_, obs_module_text("Tab.Service"));
        }

        // output
        {
            outputSettings_ = new PropertiesWidget(tab);
            updateOutputTab();
            tab->addTab(outputSettings_, obs_module_text("Tab.Output"));
        }

        QObject::connect(tab, &QTabWidget::currentChanged, [tab](int index) {
            for(int i=0; i < tab->count(); ++i) {
                auto widget = tab->widget(i);
                if(i != index)
                    widget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
            }

            auto widget = tab->widget(index);
            widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
            widget->resize(widget->minimumSizeHint());
            widget->adjustSize();

            tab->resize(tab->minimumSizeHint());
            tab->adjustSize();
        });

        tab->setCurrentIndex(0);

        return tab;
    }

    void updateServiceTab()
    {
        auto protocol_info = GetProtocolInfos()->GetInfo(config_->protocol.c_str());
        assert(protocol_info);
        if (!protocol_info) {
        	blog(LOG_ERROR, TAG "Invalid protocol \"%s\", maybe broken config file.", config_->protocol.c_str());
            protocol_info = GetProtocolInfos()->GetList();
        }

        auto service = obs_service_create(protocol_info->serviceId, ("tmp_service_" + targetid_).c_str(), from_json(config_->serviceParam), nullptr);
        serviceSettings_->UpdateProperties(
            obs_service_properties(service),
            obs_service_get_settings(service));
        obs_service_release(service);
    }

    void updateOutputTab()
    {
        auto protocol_info = GetProtocolInfos()->GetInfo(config_->protocol.c_str());
        assert(protocol_info);
        if (!protocol_info) {
        	blog(LOG_ERROR, TAG "Invalid protocol \"%s\", maybe broken config file.", config_->protocol.c_str());
            protocol_info = GetProtocolInfos()->GetList();
        }

        auto output = obs_output_create(protocol_info->outputId, ("tmp_output_" + targetid_).c_str(), from_json(config_->outputParam), nullptr);
        outputSettings_->UpdateProperties(
            obs_output_properties(output),
            obs_output_get_settings(output));
        supported_audio_encoders_ = obs_output_get_supported_audio_codecs(output);
        supported_video_encoders_ = obs_output_get_supported_video_codecs(output);
        obs_output_release(output);

        if (aenc_ && venc_)
            LoadEncoders();
    }

public:
    EditOutputWidgetImpl(const std::string& targetid, QWidget* parent = 0)
        : QDialog(parent)
        , targetid_(targetid)
    {
        auto& global = GlobalMultiOutputConfig();
        config_ = FindById(global.targets, targetid_);
        if (config_ == nullptr) {
            return;
        }
        config_ = std::make_shared<OutputTargetConfig>(*config_);

        setWindowTitle(obs_module_text("StreamingSettings"));

        scroll_ = new QScrollArea(this);
        scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAsNeeded);
        scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAsNeeded);
        scroll_->setSizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        scroll_->setSizeAdjustPolicy(QScrollArea::SizeAdjustPolicy::AdjustToContents);

        container_ = new QWidget(scroll_);
        container_->setSizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);

        auto layout = new QVBoxLayout(container_);

        int currow = 0;
        {
            auto sublayout = new QGridLayout(container_);
            sublayout->setColumnStretch(0, 0);
            sublayout->setColumnStretch(1, 1);
            sublayout->addWidget(new QLabel(obs_module_text("StreamingName"), container_), 0, 0);
            sublayout->addWidget(name_ = new QLineEdit("", container_), 0, 1);
            sublayout->addWidget(new QLabel(obs_module_text("Protocol"), container_), 1, 0);
            sublayout->addWidget(protocolSelector_ = new QComboBox(container_), 1, 1);
            layout->addLayout(sublayout);
        }
        ++currow;

        {
            auto w = CreateOutputSettingsWidget(container_);
            layout->addWidget(w);
        }
        ++currow;
        {
            auto sub_grid = new QGridLayout();
            sub_grid->setColumnStretch(0, 1);
            sub_grid->setColumnStretch(1, 0);
            {
                {
                    auto gp = new QGroupBox(obs_module_text("VideoSettings"), container_);
                    sub_grid->addWidget(gp, 0, 0, 2, 1);
                    auto encLayout = new QGridLayout();
                    int currow = 0;
                    {
                        int curcol = 0;
                        auto sublayout = new QHBoxLayout();
                        sublayout->addWidget(v_share_notify_ = new QLabel(gp), 1);
                        auto shareButton = new QPushButton(obs_module_text("Btn.EncoderShare"), gp);
                        sublayout->addWidget(shareButton);
                        encLayout->addLayout(sublayout, currow, curcol++, 1, 2);

                        QObject::connect(shareButton, &QPushButton::clicked, [this]() {
                            SaveConfig();
                            PopupShareMenu(false);
                            LoadConfig();
                            UpdateUI();
                        });
                    }
                    ++currow;
                    {
                        int curcol = 0;
                        encLayout->addWidget(new QLabel(obs_module_text("Encoder"), gp), currow, curcol++);
                        encLayout->addWidget(venc_ = new QComboBox(gp), currow, curcol++);
                    }
                    ++currow;
                    {
                        int curcol = 0;
                        encLayout->addWidget(new QLabel(obs_module_text("Scene"), gp), currow, curcol++);
                        encLayout->addWidget(v_scene_ = new QComboBox(gp), currow, curcol++);
                    }
                    ++currow;
                    {
                        int curcol = 0;
                        encLayout->addWidget(new QLabel(obs_module_text("VideoResolution"), gp), currow, curcol++);
                        encLayout->addWidget(v_resolution_ = new QLineEdit("", gp), currow, curcol++);
                        v_resolution_->setPlaceholderText(obs_module_text("SameAsOBSNow"));
                    }
                    ++currow;
                    {
                        int curcol = 0;
                        encLayout->addWidget(new QLabel(obs_module_text("VideoFPSDenumerator"), gp), currow, curcol++);
                        encLayout->addWidget(v_fpsdenumerator_ = new QComboBox(gp), currow, curcol++);
                    }
                    ++currow;
                    {
                        encLayout->addWidget(videoEncoderSettings_ = new PropertiesWidget(gp), currow, 0, 1, 2);
                        // videoEncoderSettings_->setMinimumHeight(180);
                    }
                    gp->setLayout(encLayout);
                }

                {
                    auto gp = new QGroupBox(obs_module_text("AudioSettings"), container_);
                    sub_grid->addWidget(gp, 0, 1, 1, 1);
                    auto encLayout = new QGridLayout();
                    int currow = 0;
                    {
                        int curcol = 0;
                        auto sublayout = new QHBoxLayout();
                        sublayout->addWidget(a_share_notify_ = new QLabel(gp), 1);
                        auto shareButton = new QPushButton(obs_module_text("Btn.EncoderShare"), gp);
                        sublayout->addWidget(shareButton);
                        encLayout->addLayout(sublayout, currow, curcol++, 1, 2);

                        QObject::connect(shareButton, &QPushButton::clicked, [this]() {
                            SaveConfig();
                            PopupShareMenu(true);
                            LoadConfig();
                            UpdateUI();
                        });
                    }
                    ++currow;
                    {
                        int curcol = 0;
                        encLayout->addWidget(new QLabel(obs_module_text("Encoder"), gp), currow, curcol++);
                        encLayout->addWidget(aenc_ = new QComboBox(gp), currow, curcol++);
                    }
                    ++currow;
                    {
                        int curcol = 0;
                        encLayout->addWidget(new QLabel(obs_module_text("AudioMixerID"), gp), currow, curcol++);
                        encLayout->addWidget(a_mixer_ = new QComboBox(gp), currow, curcol++);

                        for(int i = 1; i <= 6; ++i)
                            a_mixer_->addItem(QString(std::to_string(i).c_str()), i - 1);
                    }
                    ++currow;
                    {
                        encLayout->addWidget(audioEncoderSettings_ = new PropertiesWidget(gp), currow, 0, 1, 2);
                    }
                    gp->setLayout(encLayout);
                }

                {
                    auto gp = new QGroupBox(obs_module_text("OtherSettings"), container_);
                    sub_grid->addWidget(gp, 1, 1, 1, 1);
                    auto otherLayout = new QGridLayout();
                    otherLayout->addWidget(syncStart_ = new QCheckBox(obs_module_text("SyncStart"), gp), 0, 0);
                    otherLayout->addWidget(syncStop_ = new QCheckBox(obs_module_text("SyncStop"), gp), 1, 0);
                    gp->setLayout(otherLayout);
                }
            }
            layout->addLayout(sub_grid, 1);
        }
        ++currow;
        {
            auto okbtn = new QPushButton(obs_module_text("OK"), container_);
            QObject::connect(okbtn, &QPushButton::clicked, [this]() {
                SaveConfig();
                auto& global = GlobalMultiOutputConfig();
                auto it = FindById(global.targets, config_->id);
                if (it != nullptr) {
                    *it = *config_;
                }
                done(DialogCode::Accepted);
            });
            layout->addWidget(okbtn);
        }

        layout->setSizeConstraint(QLayout::SetFixedSize);
        container_->setLayout(layout);

        scroll_->setWidget(container_);
        scroll_->setWidgetResizable(true);

        auto fullLayout = new QGridLayout(this);
        fullLayout->setContentsMargins(0, 0, 0, 0);
        fullLayout->addWidget(scroll_, 0, 0);
        fullLayout->setRowStretch(0, 1);
        fullLayout->setColumnStretch(0, 1);
        setLayout(fullLayout);

        LoadProtocols();
        LoadFPSDenumerator();
        LoadEncoders();
        LoadScenes();

        LoadConfig();
        ConnectWidgetSignals();
        UpdateUI();

        auto resizeCount = std::make_shared<int>(0);
        container_->installEventFilter(new EventFilter(container_, [this, resizeCount](QObject* watched, QEvent* ev) {
            if (watched == container_ && ev->type() == QEvent::Resize) {
                ++*resizeCount;
                if (*resizeCount == 2) { // why 2? I don't know
                    auto frameGeo = frameGeometry();
                    auto clientGeo = geometry();

                    auto sizehint = container_->layout()->sizeHint();
                    // add frame size
                    sizehint = sizehint.grownBy(QMargins(
                        frameGeo.width() - clientGeo.width(),
                        frameGeo.height() - clientGeo.height(),
                        0, 0
                    ));
                    auto vs = scroll_->verticalScrollBar();
                    auto hs = scroll_->horizontalScrollBar();
                    sizehint = sizehint.grownBy(QMargins(
                        vs ? vs->width() / 2 : 0, hs ? hs->height() / 2 : 0, 
                        vs ? vs->width() / 2 : 0, hs ? hs->height() / 2 : 0
                    ));
                    auto parentCenter = parentWidget()->geometry().center();
                    QRect g;
                    g.setSize(sizehint);
                    g.moveCenter(parentCenter);
                    auto avail = screen()->availableGeometry();
                    g = avail.intersected(g);

                    // remove frame size
                    g.setTop(g.top() + (clientGeo.top() - frameGeo.top()));
                    g.setBottom(g.bottom() - (frameGeo.bottom() - clientGeo.bottom()));
                    g.setLeft(g.left() + (clientGeo.left() - frameGeo.left()));
                    g.setRight(g.right() - (frameGeo.right() - clientGeo.right()));
                    setGeometry(g);
                }
            }
            return false;
        }));
    }

    void ConnectWidgetSignals()
    {
        QObject::connect(venc_, (void (QComboBox::*)(int)) &QComboBox::currentIndexChanged, [this](){
            SaveConfig();
            LoadConfig();
            UpdateUI();
        });
        QObject::connect(aenc_, (void (QComboBox::*)(int)) &QComboBox::currentIndexChanged, [this](){
            SaveConfig();
            LoadConfig();
            UpdateUI();
        });

        QObject::connect(protocolSelector_, (void (QComboBox::*)(int)) &QComboBox::currentIndexChanged, [this](){
            blog(LOG_DEBUG, "Changing protocol to %s", tostdu8(protocolSelector_->currentText()).c_str());
            SaveConfig();
            LoadConfig();
            UpdateUI();
            updateServiceTab();
            updateOutputTab();
        });
    }

    void LoadProtocols()
    {
        auto protocol_cur = GetProtocolInfos()->GetList();
        while(protocol_cur->protocol) {
            protocolSelector_->addItem(protocol_cur->label, protocol_cur->protocol);
            ++protocol_cur;
        }
    }

    void LoadEncoders()
    {
        auto ui_text = [](auto id) {
            return std::string(obs_encoder_get_display_name(id.c_str())) + " [" + id + "]";
        };

        std::regex sp(";");
        {
            auto old_venc = venc_->currentData();
            std::sregex_token_iterator it(supported_video_encoders_.begin(), supported_video_encoders_.end(), sp, -1), itend;
            venc_->clear();
            venc_->addItem(obs_module_text("SameAsOBS"), "");
            while(it != itend) {
                for(auto x : EnumEncodersByCodec(it->str().c_str()))
                    venc_->addItem(ui_text(x).c_str(), x.c_str());
                ++it;
            }
            auto idx = venc_->findData(old_venc);
            if (idx >= 0)
                venc_->setCurrentIndex(idx);
        }

        {
            auto old_aenc = aenc_->currentData();
            std::sregex_token_iterator it(supported_audio_encoders_.begin(), supported_audio_encoders_.end(), sp, -1), itend;
            aenc_->clear();
            aenc_->addItem(obs_module_text("SameAsOBS"), "");
            while(it != itend) {
                for(auto x : EnumEncodersByCodec(it->str().c_str()))
                    aenc_->addItem(ui_text(x).c_str(), x.c_str());
                ++it;
            }
            auto idx = aenc_->findData(old_aenc);
            if (idx >= 0)
                aenc_->setCurrentIndex(idx);
        }
    }

    void LoadFPSDenumerator()
    {
        static const char* scales[] = {
            nullptr,
            (const char*)u8"1x",
            (const char*)u8"½x", 
            (const char*)u8"⅓x", 
            (const char*)u8"¼x", 
        };
        obs_video_info ovi;
        obs_get_video_info(&ovi);
        for(int i = 1; i <= 4; ++i) {
            auto fps = 1.0 * ovi.fps_num / ovi.fps_den / i;
            auto fps_decimal_part = (int)(fps * 100) - (int)((int)fps * 100);
            std::string strfps;
            if (fps_decimal_part)
                strfps = std::to_string((int)fps) + "." + std::to_string(fps_decimal_part);
            else
                strfps = std::to_string((int)fps);
            auto text = std::string(scales[i]) + " (" + strfps + " FPS)";
            v_fpsdenumerator_->addItem(text.c_str(), i);
        }
    }

    void LoadScenes()
    {
        v_scene_->addItem(obs_module_text("SameAsOBS"), "");

        using EnumParam = std::vector<std::string>;
        std::vector<std::string> scenes;
        obs_enum_scenes([](void* p, obs_source_t* src) {
            auto scenes = (EnumParam*)p;
            auto name = obs_source_get_name(src);
            if (name && *name)
                scenes->emplace_back(name);
            return true;
        }, &scenes);

        for(auto& x: scenes) {
            v_scene_->addItem(x.c_str(), x.c_str());
        }
    }

    void UpdateUI()
    {
        auto newProtocolIndex = protocolSelector_->findData(QString::fromUtf8(config_->protocol));
        // fallback to RTMP if protocol is invalid - do we really need this line?
        if (newProtocolIndex == -1) newProtocolIndex = 0;
        protocolSelector_->setCurrentIndex(newProtocolIndex);

        auto ve = venc_->currentData();
        if (ve.isValid() && ve.toString() == "")
        {
            v_scene_->setEnabled(false);
            v_resolution_->setEnabled(false);
            v_fpsdenumerator_->setEnabled(false);
        }
        else
        {
            v_scene_->setEnabled(true);
            v_resolution_->setEnabled(true);
            v_fpsdenumerator_->setEnabled(true);
        }

        auto makeShareNotify = [&](auto& targets) {
            std::string sharedTargets;
            for (auto& x : targets) {
                if (!sharedTargets.empty())
                    sharedTargets += ", ";
                sharedTargets += x;
            }
            if (sharedTargets.empty())
                sharedTargets = obs_module_text("NoShare");
            return sharedTargets;
        };

        auto sharedVideoTargets = GetEncoderShareTargets(false);
        v_share_notify_->setText(QString::fromUtf8(obs_module_text("EncoderShare") + makeShareNotify(sharedVideoTargets)));

        auto ae = aenc_->currentData();
        if (ae.isValid() && ae.toString() == "")
        {
            a_mixer_->setEnabled(false);
        }
        else
        {
            a_mixer_->setEnabled(true);
        }

        auto sharedAudioTargets = GetEncoderShareTargets(true);
        a_share_notify_->setText(QString::fromUtf8(obs_module_text("EncoderShare") + makeShareNotify(sharedAudioTargets)));
    }

    std::vector<std::string> GetEncoderShareTargets(bool isAudio) {
        std::string configId;
        if (isAudio) {
            if (!config_->audioConfig.has_value())
                return { "OBS" };
            configId = *config_->audioConfig;
        }
        else {
            if (!config_->videoConfig.has_value())
                return { "OBS" };
            configId = *config_->videoConfig;
        }
        std::vector<std::string> ret;
        auto& global = GlobalMultiOutputConfig();
        for(auto& x: global.targets) {
            if (isAudio) {
                if (x->id != config_->id && x->audioConfig.has_value() && *x->audioConfig == configId)
                    ret.emplace_back(x->name);
            }
            else {
                if (x->id != config_->id && x->videoConfig.has_value() && *x->videoConfig == configId)
                    ret.emplace_back(x->name);
            }
        }
        return ret;
    }



    void SaveVideoConfig() {
        if (!config_->videoConfig.has_value())
            return;
        
        auto& global = GlobalMultiOutputConfig();
        auto it = FindById(global.videoConfig, *config_->videoConfig);
        if (it == nullptr) {
            auto new_item = global.videoConfig.emplace_back(new VideoEncoderConfig{});
            it = new_item;
        }
        it->id = *config_->videoConfig;
        it->encoderId = tostdu8(venc_->currentData().toString());

        if (v_scene_->currentIndex() > 0)
            it->outputScene = tostdu8(v_scene_->currentData().toString());
        else
            it->outputScene.reset();
        
        auto resolution = v_resolution_->text().toUtf8();
        if (!resolution.isEmpty())
            it->resolution = resolution.constData();
        else
            it->resolution.reset();

        it->fpsDenumerator = v_fpsdenumerator_->currentData().toInt();
        
        it->encoderParams = videoEncoderSettings_->Save();
    }

    void SaveAudioConfig() {
        if (!config_->audioConfig.has_value())
            return;
        
        auto& global = GlobalMultiOutputConfig();
        auto it = FindById(global.audioConfig, *config_->audioConfig);
        if (it == nullptr) {
            auto new_item = global.audioConfig.emplace_back(new AudioEncoderConfig{});
            it = new_item;
        }
        it->id = *config_->audioConfig;
        it->encoderId = tostdu8(aenc_->currentData().toString());
        it->mixerId = a_mixer_->currentData().toInt();
        it->encoderParams = audioEncoderSettings_->Save();
    }

    void SaveConfig()
    {
        auto& global = GlobalMultiOutputConfig();

        config_->name = tostdu8(name_->text());
        config_->protocol = tostdu8(protocolSelector_->itemData(protocolSelector_->currentIndex()).toString());
        config_->syncStart = syncStart_->isChecked();
        config_->syncStop = syncStop_->isChecked();
        config_->outputParam = outputSettings_->Save();
        config_->serviceParam = serviceSettings_->Save();

        if (venc_->currentIndex() > 0 && venc_->currentData().isValid()) {
            if (!config_->videoConfig.has_value())
                config_->videoConfig = GenerateId(global);
            SaveVideoConfig();
        }
        else
        {
            config_->videoConfig.reset();
        }

        if (aenc_->currentIndex() > 0 && aenc_->currentData().isValid()) {
            if (!config_->audioConfig.has_value())
                config_->audioConfig = GenerateId(global);
            SaveAudioConfig();
        }
        else
        {
            config_->audioConfig.reset();
        }
    }


    void LoadTargetConfig(OutputTargetConfig& target) {
        name_->setText(QString::fromUtf8(target.name));
        auto protocolIndex = protocolSelector_->findData(QString::fromUtf8(target.protocol));
        if (protocolIndex < 0) protocolIndex = 0;
        protocolSelector_->setCurrentIndex(protocolIndex);
        syncStart_->setChecked(target.syncStart);
        syncStop_->setChecked(target.syncStop);
    }

    void LoadVideoConfig(VideoEncoderConfig& config) {
        {
            auto idx = venc_->findData(QString::fromUtf8(config.encoderId));
            if (idx > 0)
                venc_->setCurrentIndex(idx);
            else {
                venc_->setCurrentIndex(0);
                videoEncoderSettings_->ClearProperties();
                return;
            }
        }

        if (config.outputScene.has_value()) {
            auto idx = v_scene_->findData(QString::fromUtf8(*config.outputScene));
            if (idx >= 0)
                v_scene_->setCurrentIndex(idx);
        } else {
            v_scene_->setCurrentIndex(0);
        }

        v_resolution_->setText(QString::fromUtf8(
            config.resolution.value_or("")
        ));

        {
            auto idx = v_fpsdenumerator_->findData(config.fpsDenumerator);
            if (idx < 0)
                idx = 0;
            v_fpsdenumerator_->setCurrentIndex(idx);
        }

        {
            auto encoder = obs_video_encoder_create(config.encoderId.c_str(), ("tmp_video_encoder_" + targetid_ + "_" + config.id).c_str(), from_json(config.encoderParams), nullptr);
            videoEncoderSettings_->UpdateProperties(
                AddBF(obs_encoder_properties(encoder)),
                obs_encoder_get_settings(encoder)
            );
            obs_encoder_release(encoder);
        }
    }

    void LoadAudioConfig(AudioEncoderConfig& config) {
        {
            auto idx = aenc_->findData(QString::fromUtf8(config.encoderId));
            if (idx > 0) {
                aenc_->setCurrentIndex(idx);
            } else {
                aenc_->setCurrentIndex(0);
                audioEncoderSettings_->ClearProperties();
                return;
            }
        }

        {
            auto idx = a_mixer_->findData(config.mixerId);
            if (idx < 0)
                idx = 0;
            a_mixer_->setCurrentIndex(idx);
        }

        {
            auto encoder = obs_audio_encoder_create(config.encoderId.c_str(), ("tmp_audio_encoder_" + targetid_ + "_" + config.id).c_str(), from_json(config.encoderParams), 0, nullptr);
            audioEncoderSettings_->UpdateProperties(
                obs_encoder_properties(encoder),
                obs_encoder_get_settings(encoder)
            );
            obs_encoder_release(encoder);
        }
    }

    void LoadConfig() {
        auto& global = GlobalMultiOutputConfig();

        if (config_->name.empty())
            config_->name = obs_module_text("NewStreaming");

        LoadTargetConfig(*config_);

        bool videoConfigLoaded = false;
        if (config_->videoConfig.has_value()) {
            auto it = FindById(global.videoConfig, *config_->videoConfig);
            if (it != nullptr) {
                LoadVideoConfig(*it);
                videoConfigLoaded = true;
            }
        }
        if (!videoConfigLoaded) {
            venc_->setCurrentIndex(0);
            videoEncoderSettings_->ClearProperties();
        }

        bool audioConfigLoaded = false;
        if (config_->audioConfig.has_value()) {
            auto it = FindById(global.audioConfig, *config_->audioConfig);
            if (it != nullptr) {
                LoadAudioConfig(*it);
                audioConfigLoaded = true;
            }
        }
        if (audioConfigLoaded == false) {
            aenc_->setCurrentIndex(0);
            audioEncoderSettings_->ClearProperties();
        }

        UpdateUI();
    }
};


EditOutputWidget* createEditOutputWidget(const std::string& targetid, QWidget* parent) {
    return new EditOutputWidgetImpl(targetid, parent);
}
