#include "edit-widget.h"
#include "output-config.h"
#include "json-util.hpp"
#include "obs.hpp"
#include <QMenu>
#include <QTabWidget>
#include <qevent.h>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QScrollArea>
#include <QStandardItemModel>
#include <QSignalBlocker>

#include <regex>

#include "obs-properties-widget.h"
#include "helpers.h"
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


class ContentSizeTabWidget: public QTabWidget {
public:
    using QTabWidget::QTabWidget;

    QSize sizeHint() const override {
        return CalculateTabSize([](QWidget* widget) { return widget->sizeHint(); });
    }

    QSize minimumSizeHint() const override {
        return CalculateTabSize([](QWidget* widget) { return widget->minimumSizeHint(); });
    }

private:
    template<class T>
    QSize CalculateTabSize(T getWidgetSize) const {
        auto current = currentWidget();
        auto bar = tabBar();
        if (!current || !bar)
            return QTabWidget::sizeHint();

        auto pageSize = getWidgetSize(current);
        if (!pageSize.isValid())
            pageSize = current->sizeHint();

        auto tabSize = bar->sizeHint();
        auto result = pageSize;
        switch (tabPosition()) {
        case QTabWidget::West:
        case QTabWidget::East:
            result.rwidth() += tabSize.width();
            result.setHeight((std::max)(result.height(), tabSize.height()));
            break;
        case QTabWidget::North:
        case QTabWidget::South:
        default:
            result.setWidth((std::max)(result.width(), tabSize.width()));
            result.rheight() += tabSize.height();
            break;
        }

        const int frame = style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, this) * 2;
        result.rwidth() += frame;
        result.rheight() += frame;
        return result;
    }
};

class ContentSizeScrollArea: public QScrollArea {
public:
    using QScrollArea::QScrollArea;

    QSize sizeHint() const override {
        auto hint = viewportSizeHint();
        if (!hint.isValid())
            return QScrollArea::sizeHint();

        const int frame = frameWidth() * 2;
        hint.rwidth() += frame;
        hint.rheight() += frame;
        return hint;
    }

    QSize minimumSizeHint() const override {
        return QScrollArea::minimumSizeHint();
    }

protected:
    QSize viewportSizeHint() const override {
        auto content = widget();
        if (!content)
            return QScrollArea::viewportSizeHint();

        if (auto contentLayout = content->layout()) {
            auto hint = contentLayout->sizeHint();
            if (hint.isValid())
                return hint;
        }

        auto hint = content->sizeHint();
        if (hint.isValid())
            return hint;

        return QScrollArea::viewportSizeHint();
    }
};



class EditTargetPanelImpl: public EditTargetPanel
{
    MultiOutputConfig& globalConfig_;
    const GlobalEditSnapshot* snapshot_ = nullptr;
    OutputTargetConfigPtr config_ = nullptr;
    std::function<void()> onContentSizeChanged_;
    QTabWidget* topTabs_ = nullptr;
    QTabWidget* outputSettingsTabs_ = nullptr;

    QLineEdit* name_ = 0;

    class PropertiesWidget: public QWidget {
    public:
        PropertiesWidget(std::function<void()> onContentSizeChanged, QWidget* parent)
            : QWidget(parent)
            , onContentSizeChanged_(std::move(onContentSizeChanged))
        {
            layout_ = new QGridLayout(this);
            layout_->setColumnStretch(0, 1);
            layout_->setContentsMargins(0, 0, 0, 0);
            layout_->setSizeConstraint(QLayout::SetMinAndMaxSize);
        }

        // owner of props and settings is transfered to the created widget
        void UpdateProperties(OBSPropertiesPtr props, obs_data* settings) {
            ResetWid();
            wid_ = createPropertyWidget(std::move(props), settings_ = settings, this);
            layout_->addWidget(wid_, 0, 0, 1, 1, Qt::AlignTop);
            wid_->SetGeometryChangeCallback(onContentSizeChanged_);
            NotifyContentSizeChanged();
        }

        void ClearProperties() {
            ResetWid();
            placeholder_ = new QWidget(this);
            layout_->addWidget(placeholder_, 0, 0, 1, 1);
            NotifyContentSizeChanged();
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
        QWidget* placeholder_ = nullptr;
        OBSData settings_;
        std::function<void()> onContentSizeChanged_;

        void ResetWid() {
            if (wid_) {
                // Destroy the old property widget outside of the current
                // event handler. Tearing down OBS property widgets in the
                // middle of a button/list signal can crash Qt while child
                // widgets are being removed.
                layout_->removeWidget(wid_);
                wid_->hide();
                wid_->deleteLater();
                wid_ = nullptr;
            }
            if (placeholder_) {
                layout_->removeWidget(placeholder_);
                placeholder_->hide();
                placeholder_->deleteLater();
                placeholder_ = nullptr;
            }
        }

        void NotifyContentSizeChanged() {
            layout_->activate();
            updateGeometry();
            adjustSize();
            if (onContentSizeChanged_)
                onContentSizeChanged_();
        }
    };

    PropertiesWidget* serviceSettings_;
    PropertiesWidget* outputSettings_;
    std::string supported_video_encoders_;
    std::string supported_audio_encoders_;
    PropertiesWidget* videoEncoderSettings_;
    PropertiesWidget* audioEncoderSettings_;

    QComboBox* protocolSelector_ = 0;
    QComboBox* videoMode_ = 0;
    QComboBox* venc_ = 0;
    QComboBox* v_scene_ = 0;
    QLineEdit* v_resolution_ = 0;
    QComboBox* v_fpsdenumerator_ = 0;
    QLabel* v_share_notify_ = 0;
    QLabel* videoStatus_ = 0;
    QLabel* videoHint_ = 0;
    QWidget* videoCustomBox_ = 0;

    QComboBox* audioMode_ = 0;
    QComboBox* aenc_ = 0;
    QComboBox* a_mixer_ = 0;
    QComboBox* a_vod_track_ = nullptr;
    QLabel* a_share_notify_ = 0;
    QLabel* audioStatus_ = 0;
    QLabel* audioHint_ = 0;
    QWidget* audioCustomBox_ = 0;

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


    // "Share from another destination" clones or reuses a *plugin config*
    // id. This is distinct from sharing a live OBS encoder instance. The
    // caller owns whether globalConfig_ is a dialog draft or live config.
    template<class T>
    static std::optional<std::string> DuplicateConfig(MultiOutputConfig& global, std::list<T>& list, std::string oldid) {
        auto cfg = FindById(list, oldid);
        if (!cfg) {
            return {};
        }
        auto newid = global.GenerateId();
        auto newcfg = std::make_shared<typename T::element_type>(*cfg);
        newcfg->id = newid;
        list.push_back(newcfg);
        return newid;
    }


    // "Share from another destination" clones or reuses a *plugin config*
    // id. This is distinct from sharing a live OBS encoder instance. All
    // work happens on the dialog draft; Apply/OK commits, Cancel discards.
    void PopupShareMenu(bool isAudio) {
        auto menu = new QMenu(this);
        auto& global = globalConfig_;

        {
            auto action = menu->addAction(obs_module_text("NoShare"));
            QObject::connect(action, &QAction::triggered, [&]() {
                if (isAudio) {
                    if (!config_->audioConfig.has_value() ||
                        IsSpecialEncoder(*config_->audioConfig))
                        return;
                    auto newid = DuplicateConfig(global, global.audioConfig, *config_->audioConfig);
                    if (!newid.has_value())
                        return;
                    config_->audioConfig = *newid;
                }
                else {
                    if (!config_->videoConfig.has_value() ||
                        IsSpecialEncoder(*config_->videoConfig))
                        return;
                    auto newid = DuplicateConfig(global, global.videoConfig, *config_->videoConfig);
                    if (!newid.has_value())
                        return;
                    config_->videoConfig = *newid;
                }
                LoadConfig();
                UpdateUI();
            });
        }

        std::unordered_map<std::string, std::string> items;
        for(auto& target: global.targets) {
            auto consider = [&](const std::optional<std::string> &id) {
                if (!id.has_value() || IsSpecialEncoder(*id))
                    return;
                // Staged draft configs are committable ids too.
                auto it = items.find(*id);
                if (it != items.end())
                    it->second += ", " + target->name;
                else
                    items.insert(std::make_pair(*id, target->name));
            };
            if (isAudio)
                consider(target->id == config_->id ? config_->audioConfig : target->audioConfig);
            else
                consider(target->id == config_->id ? config_->videoConfig : target->videoConfig);
        }

        for (auto& item : items) {
            auto action = menu->addAction(QString::fromUtf8(item.second));
            QObject::connect(action, &QAction::triggered, [=]() {
                if (isAudio)
                    config_->audioConfig = item.first;
                else
                    config_->videoConfig = item.first;
                LoadConfig();
                UpdateUI();
            });
        }

        menu->exec(QCursor::pos());
    }

    void NotifyContentSizeChanged() {
        if (onContentSizeChanged_)
            onContentSizeChanged_();
    }

    QScrollArea *WrapInScroll(QWidget *page)
    {
        auto scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        page->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        scroll->setWidget(page);
        return scroll;
    }

    QTabWidget* CreateOutputSettingsWidget(QWidget* parent) {
        auto tab = outputSettingsTabs_ = new ContentSizeTabWidget(parent);

        // service
        {
            serviceSettings_ = new PropertiesWidget([this]() { NotifyContentSizeChanged(); }, tab);
            updateServiceTab();
            tab->addTab(serviceSettings_, obs_module_text("Tab.Service"));
        }

        // output
        {
            outputSettings_ = new PropertiesWidget([this]() { NotifyContentSizeChanged(); }, tab);
            updateOutputTab();
            tab->addTab(outputSettings_, obs_module_text("Tab.Output"));
        }

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

        auto service = obs_service_create(protocol_info->serviceId, ("tmp_service_" + config_->id).c_str(), from_json(config_->serviceParam), nullptr);
        OBSPropertiesPtr properties(obs_service_properties(service), [service](obs_properties_t* properties) {
            if (properties)
                obs_properties_destroy(properties);
            obs_service_release(service);
        });
        serviceSettings_->UpdateProperties(
            std::move(properties),
            obs_service_get_settings(service));
    }

    void updateOutputTab()
    {
        auto protocol_info = GetProtocolInfos()->GetInfo(config_->protocol.c_str());
        assert(protocol_info);
        if (!protocol_info) {
        	blog(LOG_ERROR, TAG "Invalid protocol \"%s\", maybe broken config file.", config_->protocol.c_str());
            protocol_info = GetProtocolInfos()->GetList();
        }

        auto output = obs_output_create(protocol_info->outputId, ("tmp_output_" + config_->id).c_str(), from_json(config_->outputParam), nullptr);
        OBSPropertiesPtr properties(obs_output_properties(output), [output](obs_properties_t* properties) {
            if (properties)
                obs_properties_destroy(properties);
            obs_output_release(output);
        });
        outputSettings_->UpdateProperties(
            std::move(properties),
            obs_output_get_settings(output));
        supported_audio_encoders_ = obs_output_get_supported_audio_codecs(output);
        supported_video_encoders_ = obs_output_get_supported_video_codecs(output);

        if (aenc_ && venc_) {
            // LoadEncoders() no longer touches the config, sync UI explicitly.
            LoadEncoders();
            LoadConfig();
            UpdateUI();
        }
    }

public:
    ~EditTargetPanelImpl() override = default;

    void FillModeBox(QComboBox *box)
    {
        box->clear();
        box->addItem(obs_module_text("Encoder.ShareStreaming"), OBS_STREAMING_ENC_PLACEHOLDER);
        box->addItem(obs_module_text("Encoder.ShareRecording"), OBS_RECORDING_ENC_PLACEHOLDER);
        box->addItem(obs_module_text("Encoder.CopyStreaming"), OBS_STREAMING_COPY_PLACEHOLDER);
        box->addItem(obs_module_text("Encoder.CopyRecording"), OBS_RECORDING_COPY_PLACEHOLDER);
        box->addItem(obs_module_text("Encoder.Custom"), "custom");
    }

    EditTargetPanelImpl(
        MultiOutputConfig& globalConfig,
        OutputTargetConfigPtr config,
        std::function<void()> onContentSizeChanged,
        const GlobalEditSnapshot* snapshot,
        QWidget* parent = 0)
        : QWidget(parent)
        , globalConfig_(globalConfig)
        , snapshot_(snapshot)
        , config_(std::move(config))
        , onContentSizeChanged_(std::move(onContentSizeChanged))
    {
        setSizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);

        auto fullLayout = new QVBoxLayout(this);
        topTabs_ = new ContentSizeTabWidget(this);
        fullLayout->addWidget(topTabs_, 1);

        // ---- Destino ----
        {
            auto page = new QWidget(this);
            auto layout = new QVBoxLayout(page);
            auto form = new QFormLayout();
            form->addRow(obs_module_text("StreamingName"), name_ = new QLineEdit(page));
            form->addRow(obs_module_text("Protocol"), protocolSelector_ = new QComboBox(page));
            layout->addLayout(form);
            layout->addWidget(CreateOutputSettingsWidget(page), 1);
            page->setLayout(layout);
            topTabs_->addTab(WrapInScroll(page), obs_module_text("Tab.Destination"));
        }
        // ---- Video ----
        {
            auto page = new QWidget(this);
            auto layout = new QVBoxLayout(page);
            auto form = new QFormLayout();
            form->addRow(obs_module_text("Encoder"), videoMode_ = new QComboBox(page));
            layout->addLayout(form);
            layout->addWidget(videoStatus_ = new QLabel(page));
            videoStatus_->setWordWrap(true);
            layout->addWidget(videoHint_ = new QLabel(page));
            videoHint_->setWordWrap(true);
            videoCustomBox_ = new QWidget(page);
            {
                auto encLayout = new QGridLayout(videoCustomBox_);
                int currow = 0;
                {
                    auto sublayout = new QHBoxLayout();
                    sublayout->addWidget(v_share_notify_ = new QLabel(videoCustomBox_), 1);
                    auto shareButton = new QPushButton(obs_module_text("Btn.EncoderShare"), videoCustomBox_);
                    sublayout->addWidget(shareButton);
                    encLayout->addLayout(sublayout, currow, 0, 1, 2);
                    QObject::connect(shareButton, &QPushButton::clicked, [this]() {
                        SaveConfig();
                        PopupShareMenu(false);
                        LoadConfig();
                        UpdateUI();
                    });
                }
                ++currow;
                encLayout->addWidget(new QLabel(obs_module_text("Encoder"), videoCustomBox_), currow, 0);
                encLayout->addWidget(venc_ = new QComboBox(videoCustomBox_), currow, 1);
                ++currow;
                encLayout->addWidget(new QLabel(obs_module_text("Scene"), videoCustomBox_), currow, 0);
                encLayout->addWidget(v_scene_ = new QComboBox(videoCustomBox_), currow, 1);
                ++currow;
                encLayout->addWidget(new QLabel(obs_module_text("VideoResolution"), videoCustomBox_), currow, 0);
                encLayout->addWidget(v_resolution_ = new QLineEdit("", videoCustomBox_), currow, 1);
                v_resolution_->setPlaceholderText(obs_module_text("SameAsOBSNow"));
                ++currow;
                encLayout->addWidget(new QLabel(obs_module_text("VideoFPSDenumerator"), videoCustomBox_), currow, 0);
                encLayout->addWidget(v_fpsdenumerator_ = new QComboBox(videoCustomBox_), currow, 1);
                ++currow;
                encLayout->addWidget(videoEncoderSettings_ = new PropertiesWidget([this]() { NotifyContentSizeChanged(); }, videoCustomBox_), currow, 0, 1, 2);
                videoCustomBox_->setLayout(encLayout);
            }
            layout->addWidget(videoCustomBox_);
            layout->addStretch(1);
            page->setLayout(layout);
            topTabs_->addTab(WrapInScroll(page), obs_module_text("Tab.Video"));
        }
        // ---- Audio ----
        {
            auto page = new QWidget(this);
            auto layout = new QVBoxLayout(page);
            auto form = new QFormLayout();
            form->addRow(obs_module_text("Encoder"), audioMode_ = new QComboBox(page));
            layout->addLayout(form);
            layout->addWidget(audioStatus_ = new QLabel(page));
            audioStatus_->setWordWrap(true);
            layout->addWidget(audioHint_ = new QLabel(page));
            audioHint_->setWordWrap(true);
            audioCustomBox_ = new QWidget(page);
            {
                auto encLayout = new QGridLayout(audioCustomBox_);
                int currow = 0;
                {
                    auto sublayout = new QHBoxLayout();
                    sublayout->addWidget(a_share_notify_ = new QLabel(audioCustomBox_), 1);
                    auto shareButton = new QPushButton(obs_module_text("Btn.EncoderShare"), audioCustomBox_);
                    sublayout->addWidget(shareButton);
                    encLayout->addLayout(sublayout, currow, 0, 1, 2);
                    QObject::connect(shareButton, &QPushButton::clicked, [this]() {
                        SaveConfig();
                        PopupShareMenu(true);
                        LoadConfig();
                        UpdateUI();
                    });
                }
                ++currow;
                encLayout->addWidget(new QLabel(obs_module_text("Encoder"), audioCustomBox_), currow, 0);
                encLayout->addWidget(aenc_ = new QComboBox(audioCustomBox_), currow, 1);
                ++currow;
                const int NUM_MIXER_TRACKS = 6;
                encLayout->addWidget(new QLabel(obs_module_text("AudioMixerID"), audioCustomBox_), currow, 0);
                encLayout->addWidget(a_mixer_ = new QComboBox(audioCustomBox_), currow, 1);
                for(int i = 1; i <= NUM_MIXER_TRACKS; ++i)
                    a_mixer_->addItem(QString(std::to_string(i).c_str()), i - 1);
                ++currow;
                encLayout->addWidget(new QLabel(obs_module_text("AudioVODTrack"), audioCustomBox_), currow, 0);
                encLayout->addWidget(a_vod_track_ = new QComboBox(audioCustomBox_), currow, 1);
                a_vod_track_->addItem("", -1);
                for(int i = 1; i <= NUM_MIXER_TRACKS; ++i)
                    a_vod_track_->addItem(QString(std::to_string(i).c_str()), i - 1);
                ++currow;
                encLayout->addWidget(audioEncoderSettings_ = new PropertiesWidget([this]() { NotifyContentSizeChanged(); }, audioCustomBox_), currow, 0, 1, 2);
                audioCustomBox_->setLayout(encLayout);
            }
            layout->addWidget(audioCustomBox_);
            layout->addStretch(1);
            page->setLayout(layout);
            topTabs_->addTab(WrapInScroll(page), obs_module_text("Tab.Audio"));
        }
        // ---- Avanzado ----
        {
            auto page = new QWidget(this);
            auto advLayout = new QGridLayout(page);
            auto gp = new QGroupBox(obs_module_text("OtherSettings"), page);
            auto otherLayout = new QGridLayout(gp);
            otherLayout->addWidget(syncStart_ = new QCheckBox(obs_module_text("SyncStart"), gp), 0, 0);
            otherLayout->addWidget(syncStop_ = new QCheckBox(obs_module_text("SyncStop"), gp), 1, 0);
            gp->setLayout(otherLayout);
            advLayout->addWidget(gp, 0, 0);
            advLayout->setRowStretch(1, 1);
            page->setLayout(advLayout);
            topTabs_->addTab(WrapInScroll(page), obs_module_text("Tab.Advanced"));
        }

        FillModeBox(videoMode_);
        FillModeBox(audioMode_);
        LoadProtocols();
        LoadFPSDenumerator();
        LoadEncoders();
        LoadScenes();

        LoadConfig();
        ConnectWidgetSignals();
        UpdateUI();
        NotifyContentSizeChanged();
    }

    void ConnectWidgetSignals()
    {
        // Draft-only: switching widgets never touches global state; Apply/OK
        // commits via CommitDraft(), Cancel discards.
        QObject::connect(videoMode_, (void (QComboBox::*)(int)) &QComboBox::currentIndexChanged, [this](){
            SaveConfig();
            LoadConfig();
            UpdateUI();
        });
        QObject::connect(audioMode_, (void (QComboBox::*)(int)) &QComboBox::currentIndexChanged, [this](){
            SaveConfig();
            LoadConfig();
            UpdateUI();
        });
        QObject::connect(venc_, (void (QComboBox::*)(int)) &QComboBox::currentIndexChanged, [this](){
            SaveConfig();
            LoadConfig();
            UpdateUI();
        });
        QObject::connect(v_scene_, (void (QComboBox::*)(int)) &QComboBox::currentIndexChanged, [this]() {
            UpdateSceneModeLock();
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

    int max_video_encoder_placeholder_index = 0;
    int max_audio_encoder_placeholder_index = 0;
    void LoadEncoders()
    {
        auto ui_text = [](auto id) {
            return std::string(obs_encoder_get_display_name(id.c_str())) + " [" + id + "]";
        };

        std::regex sp(";");
        {
            // Video encoders: custom ids only. Share/Copy modes live in the
            // Mode selector above; they are not encoder choices here.
            // Rebuilding emits currentIndexChanged; the handler would then
            // rewrite the config id and break encoder sharing.
            QSignalBlocker blocker(venc_);
            auto old_venc = venc_->currentData();
            std::sregex_token_iterator it(supported_video_encoders_.begin(), supported_video_encoders_.end(), sp, -1), itend;
            venc_->clear();
            max_video_encoder_placeholder_index = -1;

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
            // Audio encoders: custom ids only (see above).
            QSignalBlocker blocker(aenc_);
            auto old_aenc = aenc_->currentData();
            std::sregex_token_iterator it(supported_audio_encoders_.begin(), supported_audio_encoders_.end(), sp, -1), itend;
            aenc_->clear();
            max_audio_encoder_placeholder_index = -1;

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
        v_scene_->addItem(obs_module_text("SameAsOBSScene"), "");

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

    // Availability text for Share/Copy modes without a blocking modal.
    QString ModeStatusText(EncoderSourceMode mode, bool isVideo)
    {
        OBSOutputAutoRelease streamOut = obs_frontend_get_streaming_output();
        OBSOutputAutoRelease recOut = obs_frontend_get_recording_output();
        auto ready = [&](OBSOutputAutoRelease &out, bool video) -> bool {
            if (!out)
                return false;
            if (video)
                return obs_output_get_video_encoder(out) != nullptr;
            return obs_output_get_audio_encoder(out, 0) != nullptr;
        };
        switch (mode) {
        case EncoderSourceMode::ShareStreaming:
            return QString::fromUtf8(obs_module_text(
                ready(streamOut, isVideo) ? "Encoder.StatusAvailable"
                                          : "Encoder.StatusWaitingStreaming"));
        case EncoderSourceMode::ShareRecording:
            return QString::fromUtf8(obs_module_text(
                ready(recOut, isVideo) ? "Encoder.StatusAvailable"
                                       : "Encoder.StatusWaitingRecording"));
        case EncoderSourceMode::CopyStreaming:
        case EncoderSourceMode::CopyRecording:
            return QString::fromUtf8(obs_module_text("Encoder.StatusAvailable"));
        case EncoderSourceMode::Custom:
        default:
            return QString();
        }
    }

    void UpdateUI()
    {
        auto newProtocolIndex = protocolSelector_->findData(QString::fromUtf8(config_->protocol));
        // fallback to RTMP if protocol is invalid - do we really need this line?
        if (newProtocolIndex == -1) newProtocolIndex = 0;
        protocolSelector_->blockSignals(true);
        protocolSelector_->setCurrentIndex(newProtocolIndex);
        protocolSelector_->blockSignals(false);

        auto videoModeData = videoMode_->currentData().toString().toStdString();
        EncoderSourceMode videoMode = videoModeData == "custom"
                                          ? EncoderSourceMode::Custom
                                          : ResolveEncoderSourceMode(videoModeData);
        const bool customVideo = videoMode == EncoderSourceMode::Custom;
        videoCustomBox_->setVisible(customVideo);
        if (!customVideo) {
            videoStatus_->setText(ModeStatusText(videoMode, true));
            videoHint_->setText(QString::fromUtf8(obs_module_text(
                IsCopyEncoderSource(videoMode) ? "Encoder.CopyNote" : "Encoder.ShareNote")));
            v_share_notify_->setText(QString());
        } else {
            videoStatus_->setText(QString());
            videoHint_->setText(QString());
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

        if (customVideo) {
            auto sharedVideoTargets = GetEncoderShareTargets(false);
            v_share_notify_->setText(QString::fromUtf8(
                obs_module_text("EncoderShare") + makeShareNotify(sharedVideoTargets)));
        }

        auto audioModeData = audioMode_->currentData().toString().toStdString();
        EncoderSourceMode audioMode = audioModeData == "custom"
                                          ? EncoderSourceMode::Custom
                                          : ResolveEncoderSourceMode(audioModeData);
        const bool customAudio = audioMode == EncoderSourceMode::Custom;
        audioCustomBox_->setVisible(customAudio);
        if (!customAudio) {
            audioStatus_->setText(ModeStatusText(audioMode, false));
            audioHint_->setText(QString::fromUtf8(obs_module_text(
                IsCopyEncoderSource(audioMode) ? "Encoder.CopyNote" : "Encoder.ShareNote")));
            a_share_notify_->setText(QString());
        } else {
            audioStatus_->setText(QString());
            audioHint_->setText(QString());
        }

        if (customAudio) {
            auto sharedAudioTargets = GetEncoderShareTargets(true);
            a_share_notify_->setText(QString::fromUtf8(
                obs_module_text("EncoderShare") + makeShareNotify(sharedAudioTargets)));
        }

        UpdateSceneModeLock();
        UpdateEncoderEditability();
    }

    void UpdateSceneModeLock()
    {
        auto model = qobject_cast<QStandardItemModel*>(v_scene_->model());
        if (!model)
            return;

        bool locked = false;
        if (snapshot_) {
            locked = snapshot_->activeTargetIds.find(config_->id) != snapshot_->activeTargetIds.end();
            if (config_->videoConfig) {
                locked = locked || snapshot_->activeVideoConfigIds.find(*config_->videoConfig)
                    != snapshot_->activeVideoConfigIds.end();
            }
        }

        const bool usesObsScene = v_scene_->currentIndex() == 0;
        for (int row = 0; row < model->rowCount(); ++row) {
            auto item = model->item(row);
            if (!item)
                continue;
            auto flags = item->flags();
            if (locked && ((row == 0) != usesObsScene))
                flags &= ~Qt::ItemIsEnabled;
            else
                flags |= Qt::ItemIsEnabled;
            item->setFlags(flags);
        }
    }

    void UpdateEncoderEditability()
    {
        const bool targetIsActive = snapshot_
            && snapshot_->activeTargetIds.find(config_->id) != snapshot_->activeTargetIds.end();

        bool videoConfigIsActive = false;
        if (snapshot_ && config_->videoConfig) {
            videoConfigIsActive = snapshot_->activeVideoConfigIds.find(*config_->videoConfig)
                != snapshot_->activeVideoConfigIds.end();
        }

        bool audioConfigIsActive = false;
        if (snapshot_ && config_->audioConfig) {
            audioConfigIsActive = snapshot_->activeAudioConfigIds.find(*config_->audioConfig)
                != snapshot_->activeAudioConfigIds.end();
        }

        const bool videoLocked = targetIsActive || videoConfigIsActive;
        const bool audioLocked = targetIsActive || audioConfigIsActive;
        videoMode_->setEnabled(!videoLocked);
        venc_->setEnabled(!videoLocked);
        audioMode_->setEnabled(!audioLocked);
        aenc_->setEnabled(!audioLocked);
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

        if (IsSpecialEncoder(configId)) {
		    return { obs_module_text("NoShare") };
	    }

        std::vector<std::string> ret;
        auto& global = globalConfig_;
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
        if (!config_->videoConfig.has_value() || IsSpecialEncoder(*config_->videoConfig))
            return;

        auto it = globalConfig_.FindVideoConfig(*config_->videoConfig);
        if (it == nullptr) {
            auto new_item = globalConfig_.videoConfig.emplace_back(new VideoEncoderConfig{});
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
        if (!config_->audioConfig.has_value() || IsSpecialEncoder(*config_->audioConfig))
            return;

        auto it = globalConfig_.FindAudioConfig(*config_->audioConfig);
        if (it == nullptr) {
            auto new_item = globalConfig_.audioConfig.emplace_back(new AudioEncoderConfig{});
            it = new_item;
        }
        it->id = *config_->audioConfig;
        it->encoderId = tostdu8(aenc_->currentData().toString());
        it->mixerId = a_mixer_->currentData().toInt();
        it->encoderParams = audioEncoderSettings_->Save();
        
        it->audioTracks.clear();

        // First item is a placeholder
        if (a_vod_track_->currentIndex() > 0) {
            auto audio_track = std::make_shared<AudioTrackConfig>();

            audio_track->mixer_track = a_vod_track_->currentData().toInt();
            audio_track->output_track = 1;
            it->audioTracks.push_back(audio_track);
        }
    }

    void SaveConfig() override
    {
        auto& global = globalConfig_;

        config_->name = tostdu8(name_->text());
        if (protocolSelector_->currentIndex() >= 0)
            config_->protocol = tostdu8(protocolSelector_->itemData(protocolSelector_->currentIndex()).toString());
        config_->syncStart = syncStart_->isChecked();
        config_->syncStop = syncStop_->isChecked();
        if (outputSettings_)
            config_->outputParam = outputSettings_->Save();
        if (serviceSettings_)
            config_->serviceParam = serviceSettings_->Save();

        // Video: Mode selector is the source of truth. Custom reuses the
        // encoder dropdown + staged plugin config; Share/Copy store their
        // placeholder directly and skip custom persistence.
        {
            auto data = videoMode_ ? videoMode_->currentData() : QVariant();
            std::string modeId = data.isValid() ? data.toString().toStdString() : "custom";
            if (modeId == "custom") {
                if (venc_ && venc_->currentData().isValid()) {
                    if (!config_->videoConfig.has_value() ||
                        IsSpecialEncoder(*config_->videoConfig))
                        config_->videoConfig = global.GenerateId();
                    SaveVideoConfig();
                } else if (!config_->videoConfig.has_value() ||
                           IsSpecialEncoder(*config_->videoConfig)) {
                    // Custom mode with no encoder listed yet: keep unset so
                    // LoadConfig falls back to Share streaming display.
                    config_->videoConfig.reset();
                } else {
                    SaveVideoConfig();
                }
            } else {
                config_->videoConfig = modeId;
            }
        }

        {
            auto data = audioMode_ ? audioMode_->currentData() : QVariant();
            std::string modeId = data.isValid() ? data.toString().toStdString() : "custom";
            if (modeId == "custom") {
                if (aenc_ && aenc_->currentData().isValid()) {
                    if (!config_->audioConfig.has_value() ||
                        IsSpecialEncoder(*config_->audioConfig))
                        config_->audioConfig = global.GenerateId();
                    SaveAudioConfig();
                } else if (!config_->audioConfig.has_value() ||
                           IsSpecialEncoder(*config_->audioConfig)) {
                    config_->audioConfig.reset();
                } else {
                    SaveAudioConfig();
                }
            } else {
                config_->audioConfig = modeId;
            }
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

    void LoadVideoConfig(VideoEncoderConfig *pconfig) {
        if (pconfig == nullptr) {
            videoEncoderSettings_->ClearProperties();
            return;
        }
        {
            QString encoderId = QString::fromStdString(pconfig->encoderId);
            auto idx = venc_->findData(encoderId);
            if (idx >= 0) {
                venc_->blockSignals(true);
                venc_->setCurrentIndex(idx);
                venc_->blockSignals(false);
            } else {
                videoEncoderSettings_->ClearProperties();
                return;
            }
        }

        auto& config = *pconfig;

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
            auto encoder = obs_video_encoder_create(config.encoderId.c_str(), ("tmp_video_encoder_" + config_->id + "_" + config.id).c_str(), from_json(config.encoderParams), nullptr);
            OBSPropertiesPtr properties(AddBF(obs_encoder_properties(encoder)), [encoder](obs_properties_t* properties) {
                if (properties)
                    obs_properties_destroy(properties);
                obs_encoder_release(encoder);
            });
            videoEncoderSettings_->UpdateProperties(
                std::move(properties),
                obs_encoder_get_settings(encoder)
            );
        }
    }

    void LoadAudioConfig(AudioEncoderConfig *pconfig) {
        if (pconfig == nullptr) {
            audioEncoderSettings_->ClearProperties();
            return;
        }
        {
            QString encoderId = QString::fromStdString(pconfig->encoderId);
            auto idx = aenc_->findData(encoderId);
            if (idx >= 0) {
                aenc_->blockSignals(true);
                aenc_->setCurrentIndex(idx);
                aenc_->blockSignals(false);
            } else {
                audioEncoderSettings_->ClearProperties();
                return;
            }
        }

        auto& config = *pconfig;

        {
            auto idx = a_mixer_->findData(config.mixerId);
            if (idx < 0)
                idx = 0;
            a_mixer_->setCurrentIndex(idx);
        }

        {
            if (config.audioTracks.empty()) {
                a_vod_track_->setCurrentIndex(0);
            } else {
                // NOTE: at this time we assume only one audio track. This needs to be updated
                // if we ever actually add UI support for more.
                auto idx = a_vod_track_->findData(config.audioTracks.front()->mixer_track);
                if (idx < 0)
                    idx = 0;
                // This is a combobox index, not the track index
                a_vod_track_->setCurrentIndex(idx);
            }
        }

        {
            auto encoder = obs_audio_encoder_create(config.encoderId.c_str(), ("tmp_audio_encoder_" + config_->id + "_" + config.id).c_str(), from_json(config.encoderParams), 0, nullptr);
            OBSPropertiesPtr properties(obs_encoder_properties(encoder), [encoder](obs_properties_t* properties) {
                if (properties)
                    obs_properties_destroy(properties);
                obs_encoder_release(encoder);
            });
            audioEncoderSettings_->UpdateProperties(
                std::move(properties),
                obs_encoder_get_settings(encoder)
            );
        }
    }

    void SetModeBox(QComboBox *box, const std::optional<std::string> &configId)
    {
        std::string id = configId.value_or(OBS_STREAMING_ENC_PLACEHOLDER);
        int idx;
        if (IsSpecialEncoder(id))
            idx = box->findData(QString::fromUtf8(id));
        else
            idx = box->findData("custom");
        if (idx < 0)
            idx = 0;
        box->blockSignals(true);
        box->setCurrentIndex(idx);
        box->blockSignals(false);
    }

    void LoadConfig() {
        if (config_->name.empty())
            config_->name = obs_module_text("NewStreaming");

        LoadTargetConfig(*config_);

        SetModeBox(videoMode_, config_->videoConfig);
        SetModeBox(audioMode_, config_->audioConfig);

        if (!config_->videoConfig.has_value() || IsSpecialEncoder(*config_->videoConfig)) {
            videoEncoderSettings_->ClearProperties();
        } else {
            auto it = globalConfig_.FindVideoConfig(*config_->videoConfig);
            LoadVideoConfig(it ? it.get() : nullptr);
        }

        if (!config_->audioConfig.has_value() || IsSpecialEncoder(*config_->audioConfig)) {
            audioEncoderSettings_->ClearProperties();
        } else {
            auto it = globalConfig_.FindAudioConfig(*config_->audioConfig);
            LoadAudioConfig(it ? it.get() : nullptr);
        }

        UpdateUI();
    }
};


// The original single-target dialog wraps the reusable panel and provides a
// transactional draft for encoder configs: Cancel discards everything, OK or
// Apply upserts only the target and encoder configs touched by the panel.
class EditOutputWidgetImpl: public EditOutputWidget
{
    MultiOutputConfig draftConfig_;
    OutputTargetConfigPtr config_ = nullptr;
    EditTargetPanel* panel_ = nullptr;
    QDialogButtonBox* buttons_ = nullptr;

    static MultiOutputConfig CloneConfig(const MultiOutputConfig& source)
    {
        MultiOutputConfig copy;
        for (auto& target : source.targets)
            copy.targets.push_back(std::make_shared<OutputTargetConfig>(*target));
        for (auto& video : source.videoConfig)
            copy.videoConfig.push_back(std::make_shared<VideoEncoderConfig>(*video));
        for (auto& audio : source.audioConfig)
            copy.audioConfig.push_back(std::make_shared<AudioEncoderConfig>(*audio));
        return copy;
    }

    bool CommitDraft()
    {
        panel_->SaveConfig();
        auto& global = GlobalMultiOutputConfig();
        auto target = global.FindTarget(config_->id);
        if (!target)
            return false;

        *target = *config_;
        for (auto& staged : draftConfig_.videoConfig) {
            auto current = global.FindVideoConfig(staged->id);
            if (current)
                *current = *staged;
            else
                global.videoConfig.push_back(staged);
        }
        for (auto& staged : draftConfig_.audioConfig) {
            auto current = global.FindAudioConfig(staged->id);
            if (current)
                *current = *staged;
            else
                global.audioConfig.push_back(staged);
        }
        SaveMultiOutputConfig();
        return true;
    }

public:
    EditOutputWidgetImpl(const std::string& targetid, QWidget* parent = 0)
        : QDialog(parent)
    {
        auto& global = GlobalMultiOutputConfig();
        draftConfig_ = CloneConfig(global);
        config_ = draftConfig_.FindTarget(targetid);
        if (!config_)
            return;

        setWindowTitle(obs_module_text("StreamingSettings"));
        auto layout = new QVBoxLayout(this);
        panel_ = createEditTargetPanel(draftConfig_, config_, []() {}, this);
        layout->addWidget(panel_, 1);

        buttons_ = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply,
            this);
        buttons_->button(QDialogButtonBox::Ok)->setText(obs_module_text("OK"));
        buttons_->button(QDialogButtonBox::Cancel)->setText(obs_module_text("Btn.Cancel"));
        buttons_->button(QDialogButtonBox::Apply)->setText(obs_module_text("Btn.Apply"));
        QObject::connect(buttons_, &QDialogButtonBox::accepted, [this]() {
            if (CommitDraft())
                done(DialogCode::Accepted);
        });
        QObject::connect(buttons_, &QDialogButtonBox::rejected, [this]() {
            reject();
        });
        QObject::connect(buttons_->button(QDialogButtonBox::Apply), &QPushButton::clicked, [this]() {
            CommitDraft();
        });
        layout->addWidget(buttons_);
        resize(820, 640);
    }
};


EditTargetPanel* createEditTargetPanel(
    MultiOutputConfig& config,
    OutputTargetConfigPtr target,
    std::function<void()> onContentSizeChanged,
    QWidget* parent,
    const GlobalEditSnapshot* snapshot)
{
    return new EditTargetPanelImpl(config, std::move(target), std::move(onContentSizeChanged), snapshot, parent);
}

EditOutputWidget* createEditOutputWidget(const std::string& targetid, QWidget* parent) {
    return new EditOutputWidgetImpl(targetid, parent);
}
