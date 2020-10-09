#include "pch.h"
#include <regex>
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
    PushWidgetImpl(QJsonObject conf, QWidget* parent = 0)
        : QWidget(parent)
        , conf_(conf)
    {
        QObject::setObjectName("push-widget");

        timer_ = new QTimer(this);
        timer_->setInterval(std::chrono::milliseconds(1000));
        QObject::connect(timer_, &QTimer::timeout, [this]() {
            if (!output_)
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

    ~PushWidgetImpl()
    {
        ReleaseOutput();
    }

    QJsonObject Config() override
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

    void Stop() override
    {
        if (output_ && obs_output_active(output_))
        {
            obs_output_stop(output_);
        }
    }

    bool ShowEditDlg() override
    {
        auto dlg = createEditOutputWidget(conf_, this);

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

PushWidget* createPushWidget(QJsonObject conf, QWidget* parent) {
    return new PushWidgetImpl(conf, parent);
}
