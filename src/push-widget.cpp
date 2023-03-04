#include "pch.h"
#include <regex>
#include <optional>
#include "push-widget.h"
#include "edit-widget.h"

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


class PushWidgetImpl : public PushWidget, public IOBSOutputEventHanlder
{
    QJsonObject conf_;

    QPushButton* btn_ = 0;
    QLabel* name_ = 0;
    QLabel* msg_ = 0;

    using clock = std::chrono::steady_clock;
    clock::time_point begin_time_;
    clock::time_point last_info_time_;
    uint64_t total_frames_ = 0;
    uint64_t total_bytes_ = 0;
    QTimer* timer_ = 0;

    QPushButton* edit_btn_ = 0;
    QPushButton* remove_btn_ = 0;

    obs_output_t* output_ = 0;
    bool isUseDelay_ = false;

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

        // read config
        auto venc_id = QJsonUtil::Get(conf_, "v-enc", std::string{});
        auto aenc_id = QJsonUtil::Get(conf_, "a-enc", std::string{});
        auto v_bitrate = QJsonUtil::Get(conf_, "v-bitrate", 2000);
        auto a_bitrate = QJsonUtil::Get(conf_, "a-bitrate", 128);
        auto v_keyframe_sec = QJsonUtil::Get(conf_, "v-keyframe-sec", 3);
        auto a_mixer = QJsonUtil::Get(conf_, "a-mixer", 0);
        auto v_bframes = QJsonUtil::Get<int>(conf_, "v-bframes");
        auto resolution = QJsonUtil::Get<std::string>(conf_, "v-resolution");
        int v_width = -1, v_height = -1;
        
        {
            if (resolution.has_value()) {
                std::regex res_pattern(R"__(\s*(\d{1,5})\s*x\s*(\d{1,5})\s*)__");
                std::smatch match;
                if (std::regex_match(resolution.value(), match, res_pattern))
                {
                    v_width = std::stoi(match[1].str());
                    v_height = std::stoi(match[2].str());
                }

                if (a_mixer < 0 || a_mixer > 5)
                    a_mixer = 0;
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
            if (v_bframes.has_value())
                obs_data_set_int(settings, "bf", v_bframes.value());
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
        if (output_) {
            DisconnectSignals(output_);
        }

        if (output_ && obs_output_active(output_)) {
            obs_output_force_stop(output_);
        }

        if (output_ && obs_output_active(output_) == false)
        {
            bool ret = ReleaseOutputService();
            ret = ReleaseOutputEncoder() && ret;

            obs_output_release(output_);
            output_ = nullptr;

            return ret;
        }
        else if (output_) {
            obs_output_release(output_);
            output_ = nullptr;

            return true;
        }
        else if (output_ == nullptr)
            return true;
        else
            return false;
    }

    void UpdateStreamStatus() {
        using namespace std::chrono;

        if (!output_)
            return;

        static const char* units[] = {
            "bps", "Kbps", "Mbps", "Gbps", "Tbps", "Pbps", "Ebps", "Zbps", "Ybps"
        };

        auto new_bytes = obs_output_get_total_bytes(output_);
        auto new_frames = obs_output_get_total_frames(output_);
        auto now = clock::now();

        auto interval = std::chrono::duration_cast<std::chrono::duration<double>>(now - last_info_time_).count();
        if (interval > 0)
        {
            auto duration = now - begin_time_;
            auto hh = duration_cast<hours>(duration);
            duration -= hh;
            auto mm = duration_cast<minutes>(duration);
            duration -= mm;
            auto ss = duration_cast<seconds>(duration);
            duration -= ss;

            char strDuration[64] = { 0 };
            sprintf(strDuration, "%02d:%02d:%02d", (int)hh.count(), (int)mm.count(), (int)ss.count());

            char strFps[32] = { 0 };
            sprintf(strFps, "%d FPS", (int)std::round((new_frames - total_frames_) / interval));

            auto bps = (new_bytes - total_bytes_) * 8 / interval;
            auto strBps = [&]()-> std::string {
                if (bps > 0)
                {
                    int unitMaxIndex = sizeof(units) / sizeof(*units);
                    int unitIndex = static_cast<int>(log10(bps) / 3);
                    if (unitIndex >= unitMaxIndex)
                        unitIndex = unitMaxIndex - 1;
                    auto strVal = std::to_string(bps / pow(1000, unitIndex)).substr(0, 4);
                    if (!strVal.empty() && strVal.back() == '.')
                        strVal.pop_back();
                    return strVal + " " + units[unitIndex];
                }
                else
                {
                    return "0 bps";
                }
            }();
            
            msg_->setText((std::string(strDuration) + "  " + strBps + "  " + strFps).c_str());
        }

        total_frames_ = new_frames;
        total_bytes_ = new_bytes;
        last_info_time_ = now;
    }

public:
    PushWidgetImpl(QJsonObject conf, QWidget* parent = 0)
        : QWidget(parent)
        , conf_(conf)
    {
        QObject::setObjectName("push-widget");

        timer_ = new QTimer(this);
        timer_->setInterval(std::chrono::milliseconds(1000));
        QObject::connect(timer_, &QTimer::timeout, [this]() {
            UpdateStreamStatus();
        });

        auto layout = new QGridLayout(this);
        layout->addWidget(name_ = new QLabel(obs_module_text("NewStreaming"), this), 0, 0, 1, 3);
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
            if (msgbox->exec() == QMessageBox::Yes) {
                GetGlobalService().RunInUIThread([this]() {
                    delete this;
                });
            }
        });

        layout->addWidget(msg_ = new QLabel(u8"", this), 2, 0, 1, 3);
        msg_->setWordWrap(true);
        layout->addItem(new QSpacerItem(0, 10), 3, 0);
        setLayout(layout);

        LoadConfig();
    }
    
    ~PushWidgetImpl()
    {
        ReleaseOutput();
    }
    bool StartStreaming() override {
        if (IsRunning() == false)
            return true;

        // recreate output
        ReleaseOutput();

        if (output_ == nullptr)
        {
            output_ = obs_output_create("rtmp_output", "multi-output", nullptr, nullptr);
            SetAsHandler(output_);
        }

        if (output_) {
            isUseDelay_ = false;

            auto profileConfig = obs_frontend_get_profile_config();
            if (profileConfig) {
                bool useDelay = config_get_bool(profileConfig, "Output", "DelayEnable");
                bool preserveDelay = config_get_bool(profileConfig, "Output", "DelayPreserve");
                int delaySec = config_get_int(profileConfig, "Output", "DelaySec");
                obs_output_set_delay(output_,
                    useDelay ? delaySec : 0,
                    preserveDelay ? OBS_OUTPUT_DELAY_PRESERVE : 0
                );

                if (useDelay && delaySec > 0)
                    isUseDelay_ = true;
            }
        }

        if (!PrepareOutputService())
        {
            SetMsg(obs_module_text("Error.CreateRtmpService"));
            return false;
        }

        if (!PrepareOutputEncoders())
        {
            SetMsg(obs_module_text("Error.CreateEncoder"));
            return false;
        }

        if (!obs_output_start(output_))
        {
            SetMsg(obs_module_text("Error.StartOutput"));
            return false;
        }
            
            return true;
    }
    void StopStreaming() override {
        if (IsRunning() == true && output_ != nullptr)
        {
            bool useForce = false;
            if (isUseDelay_) {
                auto res = QMessageBox(QMessageBox::Icon::Information,
                    "?",
                    obs_module_text("Ques.DropDelay"),
                    QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No,
                    this
                ).exec();
                if (res == QMessageBox::Yes)
                    useForce = true;
            }

            if (!useForce)
                obs_output_stop(output_);
            else
                obs_output_force_stop(output_);
        }
    }
    QJsonObject Config() override
    {
        return conf_;
    }
   
    void OnOBSEvent(obs_frontend_event ev) override
    {
        if (ev == obs_frontend_event::OBS_FRONTEND_EVENT_EXIT
            || ev == obs_frontend_event::OBS_FRONTEND_EVENT_PROFILE_CHANGED
            || ev == obs_frontend_event::OBS_FRONTEND_EVENT_PROFILE_LIST_CHANGED
        ) {
            Stop();
        } else if (ev == obs_frontend_event::OBS_FRONTEND_EVENT_STREAMING_STARTING) {
            if (!IsRunning() && QJsonUtil::Get(conf_, "syncstart", false)) {
                StartStop();
            }
        } else if (ev == obs_frontend_event::OBS_FRONTEND_EVENT_STREAMING_STOPPING) {
            if (IsRunning() && QJsonUtil::Get(conf_, "syncstart", false)) {
                StartStop();
            }
        }
    }

    void LoadConfig()
    {
        name_->setText(QJsonUtil::Get(conf_, "name", QString("")));
    }

    void ResetInfo()
    {
        begin_time_ = clock::now();
        total_frames_ = 0;
        total_bytes_ = 0;
        last_info_time_ = clock::now();
        msg_->setText("");
    }

    bool IsRunning()
    {
        if (output_ == nullptr)
            return false;
        if (output_ != nullptr && obs_output_active(output_) == false)
            return false;
        if (output_ != nullptr && obs_output_active(output_) == true)
            return true;
        assert(false);
        return false;
    }

    void StartStop()
    {
        if (IsRunning() == false){
            // recreate output
            ReleaseOutput();

            if (output_ == nullptr)
            {
                output_ = obs_output_create("rtmp_output", "multi-output", nullptr, nullptr);
                SetAsHandler(output_);
            }

            if (output_) {
                isUseDelay_ = false;
                
                auto profileConfig = obs_frontend_get_profile_config();
                if (profileConfig) {
                    bool useDelay = config_get_bool(profileConfig, "Output", "DelayEnable");
                    bool preserveDelay = config_get_bool(profileConfig, "Output", "DelayPreserve");
                    int delaySec = config_get_int(profileConfig, "Output", "DelaySec");
                    obs_output_set_delay(output_, 
                        useDelay ? delaySec : 0,
			            preserveDelay ? OBS_OUTPUT_DELAY_PRESERVE : 0
                    );

                    if (useDelay && delaySec > 0)
                        isUseDelay_ = true;
                }
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
        else if (output_ != nullptr)
        {
            bool useForce = false;
            if (isUseDelay_) {
                auto res = QMessageBox(QMessageBox::Icon::Information, 
                    "?",
                    obs_module_text("Ques.DropDelay"), 
                    QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No,
                    this
                ).exec();
                if (res == QMessageBox::Yes)
                    useForce = true;
            }

            if (!useForce)
                obs_output_stop(output_);
            else
                obs_output_force_stop(output_);
        }
    }

    void Stop()
    {
        if (IsRunning())
        {
            obs_output_force_stop(output_);
        }
    }

    bool ShowEditDlg() override
    {
        std::unique_ptr<EditOutputWidget> dlg{ createEditOutputWidget(conf_, this) };

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
        GetGlobalService().RunInUIThread([this]()
        {
            remove_btn_->setEnabled(false);
            btn_->setText(obs_module_text("Status.Stop"));
            btn_->setEnabled(true);
            SetMsg(obs_module_text("Status.Connecting"));
            remove_btn_->setEnabled(false);
        });
    }

    void OnStarted() override
    {
        GetGlobalService().RunInUIThread([this]() {
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
        GetGlobalService().RunInUIThread([this]() {
            timer_->stop();

            remove_btn_->setEnabled(false);
            btn_->setText(obs_module_text("Status.Stop"));
            btn_->setEnabled(true);
            SetMsg(obs_module_text("Status.Reconnecting"));
        });
    }

    void OnReconnected() override
    {
        GetGlobalService().RunInUIThread([this]() {
            remove_btn_->setEnabled(false);
            btn_->setText(obs_module_text("Status.Stop"));
            btn_->setEnabled(true);
            SetMsg(obs_module_text("Status.Streaming"));

            timer_->start();
        });
    }

    void OnStopping() override
    {
        GetGlobalService().RunInUIThread([this]() {
            timer_->stop();

            remove_btn_->setEnabled(false);
            btn_->setText(obs_module_text("Status.Stop"));
            btn_->setEnabled(true);
            SetMsg(obs_module_text("Status.Stopping"));
        });
    }

    void OnStopped(int code) override
    {
        GetGlobalService().RunInUIThread([this, code]() {
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

PushWidget* createPushWidget(QJsonObject conf, QWidget* parent) {
    return new PushWidgetImpl(conf, parent);
}
