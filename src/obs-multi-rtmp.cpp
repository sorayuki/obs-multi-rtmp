#include "obs-multi-rtmp.h"
#include "obs-module.h"
#include "obs-frontend-api.h"
#include "util/config-file.h"

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

#include <list>
#include <memory>
#include <string>
#include <regex>

#define ConfigSection "obs-multi-rtmp"


std::string tostdu8(const QString& qs)
{
    auto b = qs.toUtf8();
    return std::string(b.begin(), b.end());
}


class IOBSOutputEventHanlder
{
public:
    virtual void OnStarting() {}
    static void OnOutputStarting(void* x, calldata_t* param)
    {
        auto thiz = static_cast<IOBSOutputEventHanlder*>(x);
        thiz->OnStarting();
    }

    virtual void OnStarted() {}
    static void OnOutputStarted(void* x, calldata_t* param)
    {
        auto thiz = static_cast<IOBSOutputEventHanlder*>(x);
        thiz->OnStarted();
    }

    virtual void OnStopping() {}
    static void OnOutputStopping(void* x, calldata_t* param)
    {
        auto thiz = static_cast<IOBSOutputEventHanlder*>(x);
        thiz->OnStopping();
    }

    virtual void OnStopped(int code) {}
    static void OnOutputStopped(void* x, calldata_t* param)
    {
        auto thiz = static_cast<IOBSOutputEventHanlder*>(x);
        thiz->OnStopped(calldata_int(param, "code"));
    }

    virtual void OnReconnect() {}
    static void OnOutputReconnect(void* x, calldata_t* param)
    {
        auto thiz = static_cast<IOBSOutputEventHanlder*>(x);
        thiz->OnReconnect();
    }

    virtual void OnReconnected() {}
    static void OnOutputReconnected(void* x, calldata_t* param)
    {
        auto thiz = static_cast<IOBSOutputEventHanlder*>(x);
        thiz->OnReconnected();
    }

    virtual void onDeactive() {}
    static void OnOutputDeactive(void* x, calldata_t* param)
    {
        auto thiz = static_cast<IOBSOutputEventHanlder*>(x);
        thiz->onDeactive();
    }

    void SetAsHandler(obs_output_t* output)
    {
        auto outputSignal = obs_output_get_signal_handler(output);
        if (outputSignal)
        {
            signal_handler_connect(outputSignal, "starting", &IOBSOutputEventHanlder::OnOutputStarting, this);
            signal_handler_connect(outputSignal, "start", &IOBSOutputEventHanlder::OnOutputStarted, this);
            signal_handler_connect(outputSignal, "reconnect", &IOBSOutputEventHanlder::OnOutputReconnect, this);
            signal_handler_connect(outputSignal, "reconnect_success", &IOBSOutputEventHanlder::OnOutputReconnected, this);
            signal_handler_connect(outputSignal, "stopping", &IOBSOutputEventHanlder::OnOutputStopping, this);
            signal_handler_connect(outputSignal, "deactivate", &IOBSOutputEventHanlder::OnOutputDeactive, this);
            signal_handler_connect(outputSignal, "stop", &IOBSOutputEventHanlder::OnOutputStopped, this);
        }
    }

    void DisconnectSignals(obs_output_t* output)
    {
        auto outputSignal = obs_output_get_signal_handler(output);
        if (outputSignal)
        {
            signal_handler_disconnect(outputSignal, "starting", &IOBSOutputEventHanlder::OnOutputStarting, this);
            signal_handler_disconnect(outputSignal, "start", &IOBSOutputEventHanlder::OnOutputStarted, this);
            signal_handler_disconnect(outputSignal, "reconnect", &IOBSOutputEventHanlder::OnOutputReconnect, this);
            signal_handler_disconnect(outputSignal, "reconnect_success", &IOBSOutputEventHanlder::OnOutputReconnected, this);
            signal_handler_disconnect(outputSignal, "stopping", &IOBSOutputEventHanlder::OnOutputStopping, this);
            signal_handler_disconnect(outputSignal, "deactivate", &IOBSOutputEventHanlder::OnOutputDeactive, this);
            signal_handler_disconnect(outputSignal, "stop", &IOBSOutputEventHanlder::OnOutputStopped, this);
        }
    }
};



static QThread* s_uiThread = nullptr;

template<class T>
bool RunInUIThread(T&& func)
{
    if (s_uiThread == nullptr)
        return false;
    QMetaObject::invokeMethod(s_uiThread, [func = std::move(func)]() {
        func();
    });
    return true;
}



class EditOutputWidget : public QDialog
{
    QJsonObject conf_;
    
    QLineEdit* name_ = 0;
    QLineEdit* rtmp_path_ = 0;
    QLineEdit* rtmp_key_ = 0;

    QLineEdit* rtmp_user_ = 0;
    QLineEdit* rtmp_pass_ = 0;

    QComboBox* venc_ = 0;
    QLineEdit* v_bitrate_ = 0;
    QLineEdit* v_keyframe_sec_ = 0;
    QLineEdit* v_resolution_ = 0;
    QLabel* v_warning_ = 0;
    QComboBox* aenc_ = 0;
    QLineEdit* a_bitrate_ = 0;
    QComboBox* a_mixer_ = 0;

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
            auto enc_codec = obs_get_encoder_codec(encid);
            if (strcmp(enc_codec, codec) == 0)
                result.emplace_back(encid);
        }
        return result;
    }

public:
    EditOutputWidget(QJsonObject conf, QWidget* parent = 0)
        : QDialog(parent)
        , conf_(conf)
    {
        setWindowTitle(obs_module_text("StreamingSettings"));

        auto layout = new QGridLayout(this);
        layout->setColumnStretch(0, 0);
        layout->setColumnStretch(1, 1);

        int currow = 0;
        {
            layout->addWidget(new QLabel(obs_module_text("StreamingName"), this), currow, 0);
            layout->addWidget(name_ = new QLineEdit("", this), currow, 1);
        }
        ++currow;
        {
            layout->addWidget(new QLabel(obs_module_text("StreamingServer"), this), currow, 0);
            layout->addWidget(rtmp_path_ = new QLineEdit("", this), currow, 1);
        }
        ++currow;
        {
            layout->addWidget(new QLabel(obs_module_text("StreamingKey"), this), currow, 0);
            layout->addWidget(rtmp_key_ = new QLineEdit(u8"", this), currow, 1);
        }
        ++currow;
        {
            auto sub_layout = new QGridLayout(this);
            layout->addLayout(sub_layout, currow, 0, 1, 2);

            sub_layout->setColumnStretch(0, 0);
            sub_layout->setColumnStretch(1, 1);
            sub_layout->setColumnStretch(2, 0);
            sub_layout->setColumnStretch(3, 1);
            sub_layout->addWidget(new QLabel(obs_module_text("StreamingUser"), this), 0, 0);
            sub_layout->addWidget(rtmp_user_ = new QLineEdit(u8"", this), 0, 1);
            sub_layout->addWidget(new QLabel(obs_module_text("StreamingPassword"), this), 0, 2);
            sub_layout->addWidget(rtmp_pass_ = new QLineEdit(u8"", this), 0, 3);

            rtmp_user_->setPlaceholderText(obs_module_text("UsuallyNoNeed"));
            rtmp_pass_->setPlaceholderText(obs_module_text("UsuallyNoNeed"));
            rtmp_pass_->setEchoMode(QLineEdit::Password);
        }
        ++currow;
        {
            auto sub_grid = new QGridLayout(this);
            sub_grid->setColumnStretch(0, 1);
            sub_grid->setColumnStretch(1, 1);
            layout->addLayout(sub_grid, currow, 0, 1, 2);
            {
                {
                    auto gp = new QGroupBox(obs_module_text("VideoSettings"), this);
                    sub_grid->addWidget(gp, 0, 0);
                    auto encLayout = new QGridLayout(gp);
                    int currow = 0;
                    {
                        int curcol = 0;
                        encLayout->addWidget(new QLabel(obs_module_text("Encoder"), gp), currow, curcol++);
                        encLayout->addWidget(venc_ = new QComboBox(gp), currow, curcol++);
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
                        encLayout->addWidget(new QLabel(obs_module_text("Bitrate"), gp), currow, curcol++);
                        auto c = new QGridLayout(gp);
                        {
                            int curcol = 0;
                            c->addWidget(v_bitrate_ = new QLineEdit("", gp), 0, curcol++);
                            c->addWidget(new QLabel("kbps", gp), 0, curcol++);
                            c->setColumnStretch(0, 1);
                            c->setColumnStretch(1, 0);
                        }
                        encLayout->addLayout(c, currow, curcol++);
                    }
                    ++currow;
                    {
                        int curcol = 0;
                        encLayout->addWidget(new QLabel(obs_module_text("KeyFrame"), gp), currow, curcol++);
                        auto c = new QGridLayout(gp);
                        {
                            int curcol = 0;
                            c->addWidget(v_keyframe_sec_ = new QLineEdit("", gp), 0, curcol++);
                            c->addWidget(new QLabel(obs_module_text("UnitSecond"), gp), 0, curcol++);
                            c->setColumnStretch(0, 1);
                            c->setColumnStretch(1, 0);
                        }
                        encLayout->addLayout(c, currow, curcol++);
                    }
                    ++currow;
                    {
                        encLayout->addWidget(v_warning_ = new QLabel(obs_module_text("Notice.CPUPower")), currow, 0, 1, 2);
                        v_warning_->setWordWrap(true);
                    }
                    ++currow;
                    {
                        encLayout->addWidget(new QWidget(), currow, 0);
                        encLayout->setRowStretch(currow, 1);
                    }
                    gp->setLayout(encLayout);
                }

                {
                    auto gp = new QGroupBox(obs_module_text("AudioSettings"), this);
                    sub_grid->addWidget(gp, 0, 1);
                    auto encLayout = new QGridLayout(gp);
                    int currow = 0;
                    {
                        int curcol = 0;
                        encLayout->addWidget(new QLabel(obs_module_text("Encoder"), gp), currow, curcol++);
                        encLayout->addWidget(aenc_ = new QComboBox(gp), currow, curcol++);
                    }
                    ++currow;
                    {
                        int curcol = 0;
                        encLayout->addWidget(new QLabel(obs_module_text("Bitrate"), gp), currow, curcol++);
                        auto c = new QGridLayout(gp);
                        {
                            int curcol = 0;
                            c->addWidget(a_bitrate_ = new QLineEdit(gp), 0, curcol++);
                            c->addWidget(new QLabel(obs_module_text("kbps")), 0, curcol++);
                            c->setColumnStretch(0, 1);
                            c->setColumnStretch(1, 0);
                        }
                        encLayout->addLayout(c, currow, curcol++);
                    }
                    ++currow;
                    {
                        int curcol = 0;
                        encLayout->addWidget(new QLabel(obs_module_text("AudioMixerID"), gp), currow, curcol++);
                        encLayout->addWidget(a_mixer_ = new QComboBox(gp), currow, curcol++);

                        for(int i = 1; i <= 6; ++i)
                            a_mixer_->addItem(QString(std::to_string(i).c_str()), i - 1);
                    }
                    gp->setLayout(encLayout);
                }
            }
        }
        ++currow;
        {
            auto okbtn = new QPushButton(obs_module_text("OK"), this);
            QObject::connect(okbtn, &QPushButton::clicked, [this]() {
                SaveConfig();
                done(DialogCode::Accepted);
            });
            layout->addWidget(okbtn, currow, 0, 1, 2);
        }

        resize(540, 160);
        setLayout(layout);

        LoadEncoders();
        LoadConfig();
        ConnectWidgetSignals();
        UpdateUI();
    }

    QJsonObject Config()
    {
        return conf_;
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
    }

    void LoadEncoders()
    {
        venc_->addItem(obs_module_text("SameAsOBS"), "");
        for(auto x : EnumEncodersByCodec("h264"))
            venc_->addItem(obs_encoder_get_display_name(x.c_str()), x.c_str());
        
        aenc_->addItem(obs_module_text("SameAsOBS"), "");
        for(auto x : EnumEncodersByCodec("AAC"))
            aenc_->addItem(obs_encoder_get_display_name(x.c_str()), x.c_str());
    }

    void UpdateUI()
    {
        auto ve = venc_->currentData();
        if (ve.isValid() && ve.toString() == "")
        {
            v_bitrate_->setText(obs_module_text("SameAsOBS"));
            v_bitrate_->setEnabled(false);
            v_resolution_->setText(obs_module_text("SameAsOBS"));
            v_resolution_->setEnabled(false);
            v_keyframe_sec_->setEnabled(false);
            v_keyframe_sec_->setText(obs_module_text("SameAsOBS"));
            v_warning_->setVisible(false);
        }
        else
        {
            v_bitrate_->setEnabled(true);
            v_resolution_->setEnabled(true);
            v_keyframe_sec_->setEnabled(true);
            v_warning_->setVisible(true);
        }

        auto ae = aenc_->currentData();
        if (ae.isValid() && ae.toString() == "")
        {
            a_bitrate_->setText(obs_module_text("SameAsOBS"));
            a_bitrate_->setEnabled(false);
            a_mixer_->setEnabled(false);
        }
        else
        {
            a_bitrate_->setEnabled(true);
            a_mixer_->setEnabled(true);
        }
    }

    void SaveConfig()
    {
        conf_["name"] = name_->text();
        conf_["rtmp-path"] = rtmp_path_->text();
        conf_["rtmp-key"] = rtmp_key_->text();
        conf_["rtmp-user"] = rtmp_user_->text();
        conf_["rtmp-pass"] = rtmp_pass_->text();
        conf_["v-enc"] = venc_->currentData().toString();
        conf_["a-enc"] = aenc_->currentData().toString();
        if (v_bitrate_->isEnabled())
            try { conf_["v-bitrate"] = std::stod(tostdu8(v_bitrate_->text())); } catch(...) {}
        if (v_keyframe_sec_->isEnabled())
            try { conf_["v-keyframe-sec"] = std::stod(tostdu8(v_keyframe_sec_->text())); } catch(...) {}
        if (v_resolution_->isEnabled())
            conf_["v-resolution"] = v_resolution_->text();
        if (a_bitrate_->isEnabled())
            try { conf_["a-bitrate"] = std::stod(tostdu8(a_bitrate_->text())); } catch(...) {}
        if (a_mixer_->isEnabled())
            conf_["a-mixer"] = a_mixer_->currentData().toDouble();
    }

    void LoadConfig()
    {
        auto it = conf_.find("name");
        if (it != conf_.end() && it->isString())
            name_->setText(it->toString());
        else
            name_->setText(obs_module_text("NewStreaming"));
        
        it = conf_.find("rtmp-path");
        if (it != conf_.end() && it->isString())
            rtmp_path_->setText(it->toString());
        
        it = conf_.find("rtmp-key");
        if (it != conf_.end() && it->isString())
            rtmp_key_->setText(it->toString());

        it = conf_.find("rtmp-user");
        if (it != conf_.end() && it->isString())
            rtmp_user_->setText(it->toString());

        it = conf_.find("rtmp-pass");
        if (it != conf_.end() && it->isString())
            rtmp_pass_->setText(it->toString());

        it = conf_.find("v-enc");
        if (it != conf_.end() && it->isString())
        {
            int idx = venc_->findData(it->toString());
            if (idx >= 0)
                venc_->setCurrentIndex(idx);
        }

        it = conf_.find("v-bitrate");
        if (it != conf_.end() && it->isDouble())
            v_bitrate_->setText(std::to_string((int)it->toDouble()).c_str());
        else
            v_bitrate_->setText("2000");

        it = conf_.find("v-keyframe-sec");
        if (it != conf_.end() && it->isDouble())
            v_keyframe_sec_->setText(std::to_string((int)it->toDouble()).c_str());
        else
            v_keyframe_sec_->setText("3");
        
        it = conf_.find("v-resolution");
        if (it != conf_.end() && it->isString())
            v_resolution_->setText(it->toString());
        else
            v_resolution_->setText("");
        
        it = conf_.find("a-enc");
        if (it != conf_.end() && it->isString())
        {
            int idx = aenc_->findData(it->toString());
            if (idx >= 0)
                aenc_->setCurrentIndex(idx);
        }

        it = conf_.find("a-bitrate");
        if (it != conf_.end() && it->isDouble())
            a_bitrate_->setText(std::to_string((int)it->toDouble()).c_str());
        else
            a_bitrate_->setText("128");

        it = conf_.find("a-mixer");
        {
            int dataToFind = 1;
            if (it != conf_.end() && it->isDouble())
                dataToFind = (int)it->toDouble();
            int index = a_mixer_->findData(dataToFind);
            if (index >= 0)
                a_mixer_->setCurrentIndex(index);
            else
                a_mixer_->setCurrentIndex(0);
        }
    }
};



class PushWidget : public QWidget, public IOBSOutputEventHanlder
{
    QJsonObject conf_;

    QPushButton* btn_ = 0;
    QLabel* name_ = 0;
    QLabel* fps_ = 0;
    QLabel* msg_ = 0;

    using clock = std::chrono::steady_clock;
    clock::time_point last_info_time_;
    uint64_t total_frames_ = 0;
    QTimer* timer_ = 0;

    QPushButton* edit_btn_ = 0;
    QPushButton* remove_btn_ = 0;

    obs_output_t* output_ = 0;

    bool ReleaseOutputService()
    {
        if (!output_)
            return true;
        else if (output_ && obs_output_active(output_) == false)
        {
            auto service = obs_output_get_service(output_);
            if (service)
            {
                obs_output_set_service(output_, nullptr);
                obs_service_release(service);
            }
            return true;
        }
        else
            return false;
    }
    
    bool ReleaseOutputEncoder()
    {
        if (!output_)
            return true;
        else if (output_ && obs_output_active(output_) == false)
        {
            auto venc = obs_output_get_video_encoder(output_);
            if (venc)
            {
                obs_output_set_video_encoder(output_, nullptr);
                obs_encoder_release(venc);
            }
            
            auto aenc = obs_output_get_audio_encoder(output_, 0);
            if (aenc)
            {
                obs_output_set_audio_encoder(output_, nullptr, 0);
                obs_encoder_release(aenc);
            }

            return true;
        }
        else
            return false;
    }

    bool PrepareOutputService()
    {
        if (!output_)
            return false;
        
        ReleaseOutputService();

        auto conf = obs_data_create();
        if (!conf)
            return false;
        
        obs_data_set_string(conf, "server", tostdu8(conf_["rtmp-path"].toString()).c_str());
        obs_data_set_string(conf, "key", tostdu8(conf_["rtmp-key"].toString()).c_str());

        auto user = tostdu8(conf_["rtmp-user"].toString());
        auto pass = tostdu8(conf_["rtmp-pass"].toString());
        if (!user.empty())
        {
            obs_data_set_bool(conf, "use_auth", true);
            obs_data_set_string(conf, "username", user.c_str());
            obs_data_set_string(conf, "password", pass.c_str());
        }
        else
            obs_data_set_bool(conf, "use_auth", false);
        
        auto service = obs_service_create("rtmp_custom", "multi-output-service", conf, nullptr);
        obs_data_release(conf);
        if (!service)
            return false;
        obs_output_set_service(output_, service);

        return true;
    }

    bool PrepareOutputEncoders()
    {
        if (!output_)
            return false;
        
        ReleaseOutputEncoder();

        std::string venc_id, aenc_id;
        int v_bitrate = 2000, a_bitrate = 128;
        int a_mixer = 0;
        int v_width = -1, v_height = -1;
        int v_keyframe_sec = 3;

        // read config
        {
            auto it = conf_.find("v-enc");
            if (it != conf_.end() && it->isString())
                venc_id = tostdu8(it->toString());
            it = conf_.find("a-enc");
            if (it != conf_.end() && it->isString())
                aenc_id = tostdu8(it->toString());
            it = conf_.find("v-bitrate");
            if (it != conf_.end() && it->isDouble())
                v_bitrate = (int)it->toDouble();
            it = conf_.find("a-bitrate");
            if (it != conf_.end() && it->isDouble())
                a_bitrate = (int)it->toDouble();
            it = conf_.find("v-resolution");
            if (it != conf_.end() && it->isString())
            {
                auto resstr = tostdu8(it->toString());
                std::regex res_pattern(R"__(\s*(\d{1,5})\s*x\s*(\d{1,5})\s*)__");
                std::smatch match;
                if (std::regex_match(resstr, match, res_pattern))
                {
                    v_width = std::stoi(match[1].str());
                    v_height = std::stoi(match[2].str());
                }
            }
            it = conf_.find("v-keyframe-sec");
            if (it != conf_.end() && it->isDouble())
                v_keyframe_sec = (int)it->toDouble();
            it = conf_.find("a-mixer");
            if (it != conf_.end() && it->isDouble())
            {
                int data = (int)it->toDouble();
                if (data >= 0 && data <= 5)
                    a_mixer = data;
            }
        }

        // ====== prepare encoder
        obs_encoder *venc = 0, *aenc = 0;

        // load stream encoders
        if (venc_id.empty() || aenc_id.empty())
        {
            obs_output_t* stream_out = obs_frontend_get_streaming_output();
            if (!stream_out)
            {
                auto msgbox = new QMessageBox(QMessageBox::Icon::Critical, 
                    obs_module_text("Notice.Title"), 
                    obs_module_text("Notice.GetEncoder"),
                    QMessageBox::StandardButton::Ok,
                    this
                    );
                msgbox->exec();
                return false;
            }
            if (venc_id.empty())
            {
                venc = obs_output_get_video_encoder(stream_out);
                obs_encoder_addref(venc);
            }
            if (aenc_id.empty())
            {
                aenc = obs_output_get_audio_encoder(stream_out, 0);
                obs_encoder_addref(aenc);
            }

            obs_output_release(stream_out);
        }

        // create encoders
        if (!venc)
        {
            obs_data_t* settings = obs_data_create();
            obs_data_set_int(settings, "bitrate", v_bitrate);
            obs_data_set_int(settings, "keyint_sec", v_keyframe_sec);
            venc = obs_video_encoder_create(venc_id.c_str(), "multi-rtmp-video-encoder", settings, nullptr);
            obs_data_release(settings);
            obs_encoder_set_video(venc, obs_get_video());
            if (v_width > 0 && v_height > 0)
            {
                obs_encoder_set_scaled_size(venc, v_width, v_height);
            }
        }

        if (!aenc)
        {
            obs_data_t* settings = obs_data_create();
            obs_data_set_int(settings, "bitrate", a_bitrate);
            aenc = obs_audio_encoder_create(aenc_id.c_str(), "multi-rtmp-audio-encoder", settings, a_mixer, nullptr);
            obs_data_release(settings);
            obs_encoder_set_audio(aenc, obs_get_audio());
        }

        obs_output_set_video_encoder(output_, venc);
        obs_output_set_audio_encoder(output_, aenc, 0);

        if (!aenc || !venc)
        {
            ReleaseOutputEncoder();
            return false;
        }

        return true;
    }

    bool ReleaseOutput()
    {
        if (output_ && obs_output_active(output_) == false)
        {
            bool ret = ReleaseOutputService();
            ret = ReleaseOutputEncoder() && ret;

            DisconnectSignals(output_);
            obs_output_release(output_);
            output_ = nullptr;

            return ret;
        }
        else if (output_ == nullptr)
            return true;
        else
            return false;
    }

public:
    PushWidget(QJsonObject conf, QWidget* parent = 0)
        : QWidget(parent)
        , conf_(conf)
    {
        QObject::setObjectName("push-widget");

        timer_ = new QTimer(this);
        timer_->setInterval(std::chrono::milliseconds(1000));
        QObject::connect(timer_, &QTimer::timeout, [this]() {
            if (output_)
                return;
            
            auto new_frames = obs_output_get_total_frames(output_);
            auto now = clock::now();

            auto intervalms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_info_time_).count();
            if (intervalms > 0)
            {
                auto text = std::to_string((new_frames - total_frames_) * 1000 / intervalms) + " FPS";
                fps_->setText(text.c_str());
            }

            total_frames_ = new_frames;
            last_info_time_ = now;
        });

        auto layout = new QGridLayout(this);
        layout->addWidget(name_ = new QLabel(obs_module_text("NewStreaming"), this), 0, 0, 1, 2);
        layout->addWidget(fps_ = new QLabel(u8"", this), 0, 2);
        layout->addWidget(btn_ = new QPushButton(obs_module_text("Btn.Start"), this), 1, 0);
        QObject::connect(btn_, &QPushButton::clicked, [this]() {
            StartStop();
        });

        layout->addWidget(edit_btn_ = new QPushButton(obs_module_text("Btn.Edit"), this), 1, 1);
        QObject::connect(edit_btn_, &QPushButton::clicked, [this]() {
            ShowEditDlg();
        });

        layout->addWidget(remove_btn_ = new QPushButton(obs_module_text("Btn.Delete"), this), 1, 2);
        QObject::connect(remove_btn_, &QPushButton::clicked, [this]() {
            auto msgbox = new QMessageBox(QMessageBox::Icon::Question,
                obs_module_text("Question.Title"),
                obs_module_text("Question.Delete"),
                QMessageBox::Yes | QMessageBox::No,
                this
            );
            if (msgbox->exec() == QMessageBox::Yes)
                delete this;
        });

        layout->addWidget(msg_ = new QLabel(u8"", this), 2, 0, 1, 3);
        msg_->setWordWrap(true);
        layout->addItem(new QSpacerItem(0, 10), 3, 0);
        setLayout(layout);

        LoadConfig();
    }

    ~PushWidget()
    {
        ReleaseOutput();
    }

    QJsonObject Config()
    {
        return conf_;
    }

    void LoadConfig()
    {
        auto it = conf_.find("name");
        if (it != conf_.end() && it->isString())
            name_->setText(it->toString());
    }

    void ResetInfo()
    {
        total_frames_ = 0;
        last_info_time_ = clock::now();
        fps_->setText("");
    }

    void StartStop()
    {
        if (output_ == nullptr
            || (output_ != nullptr && obs_output_active(output_) == false)
        ){
            // recreate output
            ReleaseOutput();

            if (output_ == nullptr)
            {
                output_ = obs_output_create("rtmp_output", "multi-output", nullptr, nullptr);
                SetAsHandler(output_);
            }

            if (!PrepareOutputService())
            {
                SetMsg(obs_module_text("Error.CreateRtmpService"));
                return;
            }

            if (!PrepareOutputEncoders())
            {
                SetMsg(obs_module_text("Error.CreateEncoder"));
                return;
            }

            if (!obs_output_start(output_))
            {
                SetMsg(obs_module_text("Error.StartOutput"));
            }
        }
        else
        {
            obs_output_stop(output_);
        }
    }

    void Stop()
    {
        if (output_ && obs_output_active(output_))
        {
            obs_output_stop(output_);
        }
    }

    bool ShowEditDlg()
    {
        auto dlg = new EditOutputWidget(conf_, this);

        if (dlg->exec() == QDialog::DialogCode::Accepted)
        {
            conf_ = dlg->Config();
            LoadConfig();
            return true;
        }
        else
            return false;
    }

    void SetMsg(QString msg)
    {
        msg_->setText(msg);
        msg_->setToolTip(msg);
    }

    // obs logical
    void OnStarting() override
    {
        RunInUIThread([this]()
        {
            remove_btn_->setEnabled(false);
            btn_->setEnabled(false);
            SetMsg(obs_module_text("Status.Connecting"));
            remove_btn_->setEnabled(false);
        });
    }

    void OnStarted() override
    {
        RunInUIThread([this]() {
            remove_btn_->setEnabled(false);
            btn_->setText(obs_module_text("Status.Stop"));
            btn_->setEnabled(true);
            SetMsg(obs_module_text("Status.Streaming"));

            ResetInfo();
            timer_->start();
        });
    }

    void OnReconnect() override
    {
        RunInUIThread([this]() {
            remove_btn_->setEnabled(false);
            btn_->setText(obs_module_text("Status.Stop"));
            btn_->setEnabled(true);
            SetMsg(obs_module_text("Status.Reconnecting"));
        });
    }

    void OnReconnected() override
    {
        RunInUIThread([this]() {
            remove_btn_->setEnabled(false);
            btn_->setText(obs_module_text("Status.Stop"));
            btn_->setEnabled(true);
            SetMsg(obs_module_text("Status.Streaming"));

            ResetInfo();
        });
    }

    void OnStopping() override
    {
        RunInUIThread([this]() {
            remove_btn_->setEnabled(false);
            btn_->setText(obs_module_text("Status.Stop"));
            btn_->setEnabled(false);
            SetMsg(obs_module_text("Status.Stopping"));
        });
    }

    void OnStopped(int code) override
    {
        RunInUIThread([this, code]() {
            ResetInfo();
            timer_->stop();

            remove_btn_->setEnabled(true);
            btn_->setText(obs_module_text("Btn.Start"));
            btn_->setEnabled(true);
            SetMsg(u8"");

            switch(code)
            {
                case 0:
                    SetMsg(u8"");
                    break;
                case -1:
                    SetMsg(obs_module_text("Error.WrongRTMPUrl"));
                    break;
                case -2:
                    SetMsg(obs_module_text("Error.ServerConnect"));
                    break;
                case -3:
                    SetMsg(obs_module_text("Error.ServerHandshake"));
                    break;
                case -4:
                    SetMsg(obs_module_text("Error.ServerRefuse"));
                    break;
                default:
                    SetMsg(obs_module_text("Error.Unknown"));
                    break;
            }
        });

        ReleaseOutputEncoder();
    }
};



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
            auto pushwidget = new PushWidget(QJsonObject(), container_);
            layout_->addWidget(pushwidget);
            if (!pushwidget->ShowEditDlg())
                delete pushwidget;
        });
        layout_->addWidget(addButton_);

        if (std::string(u8"多路推流") == obs_module_text("Title"))
            layout_->addWidget(new QLabel(u8"本插件免费提供，请不要为此付费。\n作者：雷鸣", container_));
        else
            layout_->addWidget(new QLabel(u8"This plugin provided free of charge. \nAuthor: SoraYuki", container_));

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
                this).exec();
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
                auto w = static_cast<PushWidget*>(c);
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

    void SaveConfig()
    {
        QJsonArray targetlist;
        for(auto x : GetAllPushWidgets())
            targetlist.append(x->Config());
        QJsonObject root;
        root["targets"] = targetlist;
        QJsonDocument jsondoc;
        jsondoc.setObject(root);
        config_set_string(obs_frontend_get_global_config(), ConfigSection, "json", jsondoc.toBinaryData().toBase64());

        config_set_int(obs_frontend_get_global_config(), ConfigSection, "DockLocation", (int)dockLocation_);
        config_set_bool(obs_frontend_get_global_config(), ConfigSection, "DockVisible", dockVisible_);
    }

    void LoadConfig()
    {
        QJsonObject conf;
        auto base64str = config_get_string(obs_frontend_get_global_config(), ConfigSection, "json");
        if (base64str && *base64str)
        {
            auto bindat = QByteArray::fromBase64(base64str);
            auto jsondoc = QJsonDocument::fromBinaryData(bindat);
            if (jsondoc.isObject())
                conf = jsondoc.object();
        }

        auto targets = conf.find("targets");
        if (targets != conf.end() && targets->isArray())
        {
            for(auto x : targets->toArray())
            {
                if (x.isObject())
                {
                    auto pushwidget = new PushWidget(((QJsonValue)x).toObject(), container_);
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
    if (obs_get_version() < MAKE_SEMANTIC_VERSION(25, 0, 0))
        return false;
    
    auto mainwin = (QMainWindow*)obs_frontend_get_main_window();
    if (mainwin == nullptr)
        return false;
    QMetaObject::invokeMethod(mainwin, []() {
        s_uiThread = QThread::currentThread();
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
                static_cast<MultiOutputWidget*>(private_data)->SaveConfig();
                static_cast<MultiOutputWidget*>(private_data)->StopAll();
            }
        }, dock
    );

    return true;
}

const char *obs_module_description(void)
{
    return "Multiple RTMP Output Plugin";
}
