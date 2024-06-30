#include "pch.h"
#include <regex>
#include <optional>
#include "push-widget.h"
#include "edit-widget.h"
#include "output-config.h"
#include "protocols.h"

#include "obs.hpp"

class IOBSOutputEventHanlder
{
public:
    virtual void OnStarting() {}
    static void OnOutputStarting(void* x, calldata_t*)
    {
        auto thiz = static_cast<IOBSOutputEventHanlder*>(x);
        thiz->OnStarting();
    }

    virtual void OnStarted() {}
    static void OnOutputStarted(void* x, calldata_t*)
    {
        auto thiz = static_cast<IOBSOutputEventHanlder*>(x);
        thiz->OnStarted();
    }

    virtual void OnStopping() {}
    static void OnOutputStopping(void* x, calldata_t*)
    {
        auto thiz = static_cast<IOBSOutputEventHanlder*>(x);
        thiz->OnStopping();
    }
   
    virtual void OnStopped(int) {}
    static void OnOutputStopped(void* x, calldata_t* param)
    {
        auto thiz = static_cast<IOBSOutputEventHanlder*>(x);
        thiz->OnStopped(calldata_int(param, "code"));
    }

    virtual void OnReconnect() {}
    static void OnOutputReconnect(void* x, calldata_t*)
    {
        auto thiz = static_cast<IOBSOutputEventHanlder*>(x);
        thiz->OnReconnect();
    }

    virtual void OnReconnected() {}
    static void OnOutputReconnected(void* x, calldata_t*)
    {
        auto thiz = static_cast<IOBSOutputEventHanlder*>(x);
        thiz->OnReconnected();
    }

    virtual void onDeactive() {}
    static void OnOutputDeactive(void* x, calldata_t*)
    {
        auto thiz = static_cast<IOBSOutputEventHanlder*>(x);
        thiz->onDeactive();
    }

    void SetMeAsHandler(obs_output_t* output)
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
    std::string targetid_;
    OutputTargetConfigPtr config_;

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
    bool using_main_video_encoder_ = true;
    bool using_main_audio_encoder_ = true;
    obs_view_t* scene_view_ = 0;
    bool isUseDelay_ = false;


    bool PrepareOutputService()
    {
        if (!output_) {
            blog(LOG_ERROR, TAG "Prepare output service before output object is created.");
            return false;
        }
        
        ReleaseOutputService();
        
        auto conf = obs_data_create_from_json(config_->serviceParam.dump().c_str());

        auto protocolInfo = GetProtocolInfos()->GetInfo(config_->protocol.c_str());
        assert(protocolInfo);
        if (!protocolInfo) {
        	blog(LOG_ERROR, TAG "Invalid protocol \"%s\", maybe broken config file.", config_->protocol.c_str());
        	return false;
        }
        auto service_id = protocolInfo->serviceId;

        if (!conf)
            return false;
        
        auto service = obs_service_create(service_id, "multi-output-service", conf, nullptr);
        obs_data_release(conf);
        if (!service)
            return false;
        obs_output_set_service(output_, service);

        return true;
    }


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
        else {
            return false;
        }
    }


    bool PrepareEncoderSource() {
        if (!output_) {
            blog(LOG_ERROR, TAG "Prepare output scene before output object is created.");
            return false;
        }

        if (!using_main_video_encoder_) {
            auto venc = obs_output_get_video_encoder(output_);
            if (!venc) {
                blog(LOG_ERROR, TAG "Prepare output scene before encoder is created.");
                return false;
            }
            auto videoConfig = FindById(GlobalMultiOutputConfig().videoConfig, config_->videoConfig.value_or(""));

            if (!videoConfig || !videoConfig->outputScene.has_value()) {
                obs_encoder_set_video(venc, obs_get_video());
            } else {
                auto sceneName = *videoConfig->outputScene;
                OBSSourceAutoRelease scene = obs_get_source_by_name(sceneName.c_str());
                if (scene == nullptr) {
                    blog(LOG_ERROR, TAG "Output scene is not found.");
                    return false;
                }
                ReleaseOutputSceneView();

                scene_view_ = obs_view_create();
                obs_view_set_source(scene_view_, 0, scene);
                obs_source_inc_active(scene);
                auto scene_video = obs_view_add(scene_view_);
                obs_encoder_set_video(venc, scene_video);
            }
        }

        if (!using_main_audio_encoder_) {
            auto aenc = obs_output_get_audio_encoder(output_, 0);
            if (!aenc) {
                blog(LOG_ERROR, TAG "Prepare output scene before encoder is created.");
                return false;
            }
            obs_encoder_set_audio(aenc, obs_get_audio());
        }
        
        return true;
    }


    bool ReleaseOutputSceneView() {
        if (!scene_view_)
            return true;

        obs_view_remove(scene_view_);
        OBSSourceAutoRelease source = obs_view_get_source(scene_view_, 0);
        if (source) {
            obs_source_dec_active(source);
        }
        obs_view_set_source(scene_view_, 0, nullptr);
        obs_view_destroy(scene_view_);
        scene_view_ = nullptr;

        return true;
    }


    std::string VideoEncoderName() {
        return "multi-rtmp-venc" + config_->videoConfig.value_or("");
    }

    std::string AudioEncoderName() {
        return "multi-rtmp-aenc" + config_->audioConfig.value_or("");
    }


    bool PrepareOutputEncoders()
    {
        if (!output_) {
            blog(LOG_ERROR, TAG "Prepare output encoder before output object is created.");
            return false;
        }
        
        ReleaseOutputEncoder();

        auto& global = GlobalMultiOutputConfig();

        // main output
        OBSOutput mainOutput = obs_frontend_get_streaming_output();
        obs_output_release(mainOutput);

        // video encoder
        OBSEncoder venc;
        if (config_->videoConfig.has_value()) {
            // find shared video encoder or create new
            venc = obs_get_encoder_by_name(VideoEncoderName().c_str());
            if (venc) {
                obs_encoder_release(venc);
            } else {
                auto videoConfigId = *config_->videoConfig;
                auto videoConfig = FindById(global.videoConfig, videoConfigId);
                if (videoConfig) {
                    OBSData settings = obs_data_create_from_json(videoConfig->encoderParams.dump().c_str());
                    obs_data_release(settings);
                    venc = obs_video_encoder_create(videoConfig->encoderId.c_str(), VideoEncoderName().c_str(), settings, nullptr);
                    obs_encoder_release(venc);
                    if (venc) {
                        if (videoConfig->resolution.has_value()) {
                            std::regex res_pattern(R"__(\s*(\d{1,5})\s*x\s*(\d{1,5})\s*)__");
                            std::smatch match;
                            if (std::regex_match(*videoConfig->resolution, match, res_pattern))
                            {
                                auto width = std::stoi(match[1].str());
                                auto height = std::stoi(match[2].str());
                                obs_encoder_set_gpu_scale_type(venc, obs_scale_type::OBS_SCALE_BICUBIC);
                                obs_encoder_set_scaled_size(venc, width, height);
                            }
                        }
                        obs_encoder_set_frame_rate_divisor(venc, videoConfig->fpsDenumerator);
                        using_main_video_encoder_ = false;
                    }
                } else {
                    blog(LOG_ERROR, TAG "Load video encoder config failed for %s. Sharing with main output.", config_->name.c_str());
                }
            }
        } else {
            venc = obs_output_get_video_encoder(mainOutput);
            using_main_video_encoder_ = true;
        }

        // audio encoder
        OBSEncoder aenc;
        if (config_->audioConfig.has_value()) {
            // find shared audio encoder or create new
            aenc = obs_get_encoder_by_name(AudioEncoderName().c_str());
            if (aenc)
                obs_encoder_release(aenc);
            else {
                auto audioConfigId = *config_->audioConfig;
                auto audioConfig = FindById(global.audioConfig, audioConfigId);
                if (audioConfig) {
                    OBSData settings = obs_data_create_from_json(audioConfig->encoderParams.dump().c_str());
                    obs_data_release(settings);
                    aenc = obs_audio_encoder_create(audioConfig->encoderId.c_str(), AudioEncoderName().c_str(), settings, audioConfig->mixerId, nullptr);
                    obs_encoder_release(aenc);

                    using_main_audio_encoder_ = false;
                } else {
                    blog(LOG_ERROR, TAG "Load audio encoder config failed for %s. Sharing with main output.", config_->name.c_str());
                }
            }
        } else {
            aenc = obs_output_get_audio_encoder(mainOutput, 0);
            using_main_audio_encoder_ = true;
        }

        if (!aenc || !venc) {
            ReleaseOutputEncoder();

            auto msgbox = new QMessageBox(QMessageBox::Icon::Critical, 
                obs_module_text("Notice.Title"), 
                obs_module_text("Notice.GetEncoder"),
                QMessageBox::StandardButton::Ok,
                this
                );
            msgbox->exec();
            return false;
        }

        obs_output_set_audio_encoder(output_, obs_encoder_get_ref(aenc), 0);
        obs_output_set_video_encoder(output_, obs_encoder_get_ref(venc));

        return true;
    }


    bool ReleaseOutputEncoder()
    {
        if (!output_)
            return true;
        else if (obs_output_active(output_) == false)
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
        else {
            blog(LOG_ERROR, TAG "Release output while it is active.");
            return false;
        }
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

            ret = ReleaseOutputSceneView() && ret;

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
            sprintf(strFps, "%d FPS", static_cast<int>(std::round((new_frames - total_frames_) / interval)));

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
    PushWidgetImpl(const std::string& targetid, QWidget* parent = 0)
        : QWidget(parent)
        , targetid_(targetid)
    {
        QObject::setObjectName("push-widget");

        auto& global = GlobalMultiOutputConfig();
        config_ = FindById(global.targets, targetid_);
        if (!config_)
            return;

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
                    auto& global = GlobalMultiOutputConfig();
                    auto it = std::find_if(global.targets.begin(), global.targets.end(), [&](auto& x) { return x->id == targetid_; });
                    if (it != global.targets.end()) {
                        global.targets.erase(it);
                    }
                    delete this;
                    SaveMultiOutputConfig();
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


    void StartStreaming() override {
        if (IsRunning())
            return;
        
        // recreate output
        ReleaseOutput();

        if (output_ == nullptr)
        {
            obs_data* output_settings = obs_data_create_from_json(config_->outputParam.dump().c_str());
            
            auto protocolInfo = GetProtocolInfos()->GetInfo(config_->protocol.c_str());
            assert(protocolInfo);
            if (!protocolInfo) {
	        	blog(LOG_ERROR, TAG "Invalid protocol \"%s\", maybe broken config file.", config_->protocol.c_str());
	        	protocolInfo = GetProtocolInfos()->GetList();
	        }
            auto output_id = protocolInfo->outputId;

            blog(LOG_DEBUG, "Streaming to output: %s", output_id);

            output_ = obs_output_create(output_id, "multi-output", output_settings, nullptr);
            SetMeAsHandler(output_);
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

        if (!PrepareEncoderSource())
        {
            SetMsg(obs_module_text("Error.SceneNotExist"));
            return;
        }

        if (!obs_output_start(output_))
        {
            SetMsg(obs_module_text("Error.StartOutput"));
        }
    }

    void StopStreaming() override {
        if (!IsRunning())
            return;
        
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
   
    void OnOBSEvent(obs_frontend_event ev) override
    {
        if (ev == obs_frontend_event::OBS_FRONTEND_EVENT_EXIT
            || ev == obs_frontend_event::OBS_FRONTEND_EVENT_PROFILE_CHANGED
            || ev == obs_frontend_event::OBS_FRONTEND_EVENT_PROFILE_LIST_CHANGED
        ) {
            Stop();
        } else if (ev == obs_frontend_event::OBS_FRONTEND_EVENT_STREAMING_STARTING) {
            if (!IsRunning() && config_->syncStart) {
                StartStop();
            }
        } else if (ev == obs_frontend_event::OBS_FRONTEND_EVENT_STREAMING_STOPPING) {
            if (IsRunning() && config_->syncStop) {
                StartStop();
            }
        }
    }

    void LoadConfig()
    {
        name_->setText(QString::fromUtf8(config_->name));
    }

    void ResetInfo()
    {
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
        if (IsRunning() == false)
        {
            StartStreaming();
        }
        else if (output_ != nullptr)
        {
            StopStreaming();
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
        std::unique_ptr<EditOutputWidget> dlg{ createEditOutputWidget(targetid_, (QMainWindow*)obs_frontend_get_main_window()) };

        if (dlg->exec() == QDialog::DialogCode::Accepted)
        {
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
        GetGlobalService().RunInUIThread([this]() {
            begin_time_ = clock::now();
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

            ResetInfo();
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
        ReleaseOutputSceneView();
    }
};

PushWidget* createPushWidget(const std::string& targetid, QWidget* parent) {
    return new PushWidgetImpl(targetid, parent);
}
