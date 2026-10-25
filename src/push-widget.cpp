#include "pch.h"
#include "helpers.h"
#include <regex>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <tuple>
#include <unordered_map>
#include "push-widget.h"
#include "output-config.h"
#include "protocols.h"

#include "obs.hpp"

static nlohmann::json OBSDataToJson(obs_data_t* data)
{
    if (!data)
        return nlohmann::json::object();

    auto serialized = obs_data_get_json(data);
    if (!serialized)
        return nlohmann::json::object();

    try {
        return nlohmann::json::parse(serialized);
    } catch (...) {
        return nlohmann::json::object();
    }
}

static void UpdateEncoderSettings(
    obs_encoder_t* encoder,
    const nlohmann::json& oldParams,
    const nlohmann::json& newParams,
    const char* encoderType)
{
    if (!encoder)
        return;

    OBSDataAutoRelease settings = obs_encoder_get_settings(encoder);
    OBSDataAutoRelease defaults = obs_encoder_get_defaults(encoder);
    auto currentJson = OBSDataToJson(settings);
    auto defaultsJson = OBSDataToJson(defaults);
    auto merged = newParams.is_object() ? newParams : nlohmann::json::object();

    if (oldParams.is_object()) {
        for (const auto& [key, oldValue] : oldParams.items()) {
            if (merged.contains(key))
                continue;
            if (defaultsJson.contains(key)) {
                merged[key] = defaultsJson[key];
            } else {
                blog(LOG_WARNING, TAG "%s parameter '%s' has no default; keeping its current value", encoderType, key.c_str());
                merged[key] = currentJson.value(key, oldValue);
            }
        }
    }

    OBSDataAutoRelease updatedSettings = obs_data_create_from_json(merged.dump().c_str());
    if (!settings || !updatedSettings)
        return;

    obs_data_clear(settings);
    obs_data_apply(settings, updatedSettings);
    obs_encoder_update(encoder, settings);

class SharedSceneView final
{
    OBSEncoder encoder_;
    obs_view_t* view_ = nullptr;
    video_t* video_ = nullptr;
    std::mutex source_mutex_;

public:
    explicit SharedSceneView(obs_encoder_t* encoder)
        : encoder_(encoder)
    {
    }

    ~SharedSceneView()
    {
        if (!view_)
            return;

        obs_view_remove(view_);
        OBSSourceAutoRelease source = obs_view_get_source(view_, 0);
        if (source)
            obs_source_dec_active(source);
        obs_view_set_source(view_, 0, nullptr);
        obs_view_destroy(view_);
    }

    bool Initialize(obs_source_t* source)
    {
        if (!source)
            return false;

        view_ = obs_view_create();
        if (!view_)
            return false;

        obs_view_set_source(view_, 0, source);
        obs_source_inc_active(source);
        video_ = obs_view_add(view_);
        return video_ != nullptr;
    }

    bool SetSource(obs_source_t* source)
    {
        if (!view_ || !source)
            return false;

        std::lock_guard<std::mutex> lock(source_mutex_);
        OBSSourceAutoRelease current = obs_view_get_source(view_, 0);
        if (current == source)
            return true;

        obs_view_set_source(view_, 0, source);
        if (current)
            obs_source_dec_active(current);
        obs_source_inc_active(source);
        return true;
    }

    video_t* Video() const
    {
        return video_;
    }
};

class SharedSceneViewManager final
{
    std::mutex mutex_;
    std::unordered_map<obs_encoder_t*, std::weak_ptr<SharedSceneView>> entries_;

public:
    std::shared_ptr<SharedSceneView> Acquire(obs_encoder_t* encoder, obs_source_t* source)
    {
        if (!encoder || !source)
            return {};

        std::unique_lock<std::mutex> lock(mutex_);
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (it->second.expired())
                it = entries_.erase(it);
            else
                ++it;
        }

        auto it = entries_.find(encoder);
        if (it != entries_.end()) {
            auto entry = it->second.lock();
            if (entry) {
                lock.unlock();
                if (!entry->SetSource(source)) {
                    blog(LOG_ERROR, TAG "Could not update the shared output scene.");
                    return {};
                }
                return entry;
            }
            entries_.erase(it);
        }

        if (obs_encoder_active(encoder)) {
            blog(LOG_ERROR, TAG "Shared video encoder is active without a registered scene view.");
            return {};
        }

        auto entry = std::make_shared<SharedSceneView>(encoder);
        if (!entry->Initialize(source)) {
            lock.unlock();
            entry.reset();
            blog(LOG_ERROR, TAG "Could not create the shared output scene view.");
            return {};
        }

        obs_encoder_set_video(encoder, entry->Video());
        if (obs_encoder_parent_video(encoder) != entry->Video()) {
            lock.unlock();
            entry.reset();
            blog(LOG_ERROR, TAG "Could not bind the shared scene view to the video encoder.");
            return {};
        }

        entries_[encoder] = entry;
        return entry;
    }
};

static SharedSceneViewManager& GetSharedSceneViewManager()
{
    static SharedSceneViewManager manager;
    return manager;
}

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
    QLabel* status_dot_ = 0;
    QCheckBox* enabled_cb_ = 0;

    using clock = std::chrono::steady_clock;
    clock::time_point begin_time_;
    clock::time_point last_info_time_;
    uint64_t total_frames_ = 0;
    uint64_t total_bytes_ = 0;
    QTimer* timer_ = 0;

    obs_output_t* output_ = 0;
    // Owned reference to the service handed to output_ (the output itself only keeps a weak back-pointer).
    obs_service_t* service_ = 0;
    bool using_main_video_encoder_ = false;
    bool using_main_audio_encoder_ = false;
    std::shared_ptr<SharedSceneView> scene_view_;
    bool isUseDelay_ = false;
    std::atomic_bool isStarting_ = false;

    bool isConnecting_ = false;
    bool isReconnecting_ = false;
    int lastErrorCode_ = 0;
    std::atomic_bool isBusy_ = false;

    // Registered under the stable target id (not the display name) so key
    // bindings survive renames. Their description label is set once at
    // registration time and won't update if the target is renamed later.
    obs_hotkey_id startHotkeyId_ = OBS_INVALID_HOTKEY_ID;
    obs_hotkey_id stopHotkeyId_ = OBS_INVALID_HOTKEY_ID;

    bool PrepareOutputService()
    {
        if (!output_) {
            blog(LOG_ERROR, TAG "Prepare output service before output object is created.");
            return false;
        }

        // The output is rebuilt per start, so no service should be left over.
        // Freeing one here would be unsafe: libobs ignores
        // obs_output_set_service(output, nullptr), so the output would keep a
        // dangling pointer into the freed service.
        if (service_) {
            blog(LOG_ERROR, TAG "Stale service object before creating a new one.");
            return false;
        }

        // Auto-release: obs_service_create keeps its own reference and every
        // early return below would otherwise leak this one.
        OBSDataAutoRelease conf = obs_data_create_from_json(config_->serviceParam.dump().c_str());

        auto protocolInfo = GetProtocolInfos()->GetInfo(config_->protocol.c_str());
        assert(protocolInfo);
        if (!protocolInfo) {
        	blog(LOG_ERROR, TAG "Invalid protocol \"%s\", maybe broken config file.", config_->protocol.c_str());
        	return false;
        }
        auto service_id = protocolInfo->serviceId;

        if (!conf)
            return false;
        
        // We own this reference. The output only keeps a weak back-pointer,
        // which obs_output_destroy() clears again.
        obs_service_t* service = obs_service_create(service_id, "multi-output-service", conf, nullptr);
        if (!service)
            return false;
        obs_output_set_service(output_, service);
        service_ = service;

        return true;
    }


    // Only valid once output_ is gone: destroying the output is what clears the
    // service's back-pointer, so freeing it earlier leaves a dangling pointer.
    bool ReleaseOutputService()
    {
        if (!service_)
            return true;

        obs_service_release(service_);
        service_ = nullptr;
        return true;
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
            auto videoConfig = GlobalMultiOutputConfig().FindVideoConfig(config_->videoConfig.value_or(""));

            if (!videoConfig || !videoConfig->outputScene.has_value()) {
                obs_encoder_set_video(venc, obs_get_video());
            } else {
                auto sceneName = *videoConfig->outputScene;
                OBSSourceAutoRelease scene = obs_get_source_by_name(sceneName.c_str());
                if (scene == nullptr) {
                    blog(LOG_ERROR, TAG "Output scene is not found.");
                    return false;
                }
                scene_view_ = GetSharedSceneViewManager().Acquire(venc, scene);
                if (!scene_view_)
                    return false;
            }
        }

        if (!using_main_audio_encoder_) {
            auto aenc = obs_output_get_audio_encoder(output_, 0);
            if (!aenc) {
                blog(LOG_ERROR, TAG "Prepare output scene before encoder is created.");
                return false;
            }
            obs_encoder_set_audio(aenc, obs_get_audio());

            // Set other audio tracks
            auto audioConfig = GlobalMultiOutputConfig().FindAudioConfig(config_->audioConfig.value_or(""));
            if (audioConfig) {
                for (auto& track : audioConfig->audioTracks) {
                    auto enc = obs_output_get_audio_encoder(output_, track->output_track);
                    if (enc) {
                        obs_encoder_set_audio(enc, obs_get_audio());
                    }
                }
            }
        }
        
        return true;
    }


    bool ReleaseOutputSceneView() {
        scene_view_.reset();
        return true;
    }


    std::string VideoEncoderName() {
        return "multi-rtmp-venc" + config_->videoConfig.value_or("");
    }

    std::string AudioEncoderName(int track) {
        return "multi-rtmp-aenc" + config_->audioConfig.value_or("") + "-track-idx-" + std::to_string(track);
    }

    std::optional<std::tuple<int, int>> ParseResolution(const std::optional<std::string>& res) {
        if (!res.has_value())
            return std::nullopt;
        std::regex res_pattern(R"__(\s*(\d{1,5})\s*x\s*(\d{1,5})\s*)__");
        std::smatch match;
        if (std::regex_match(*res, match, res_pattern))
        {
            auto width = std::stoi(match[1].str());
            auto height = std::stoi(match[2].str());
            return {{ width, height }};
        }

        return std::nullopt;
    }

    static bool VideoConfigEqual(const VideoEncoderConfig& left, const VideoEncoderConfig& right)
    {
        return left.encoderId == right.encoderId
            && left.fpsDenumerator == right.fpsDenumerator
            && left.encoderParams == right.encoderParams
            && left.outputScene == right.outputScene
            && left.resolution == right.resolution;
    }

    static bool AudioConfigEqual(const AudioEncoderConfig& left, const AudioEncoderConfig& right)
    {
        if (left.encoderId != right.encoderId
            || left.encoderParams != right.encoderParams
            || left.mixerId != right.mixerId
            || left.audioTracks.size() != right.audioTracks.size())
            return false;

        auto leftTrack = left.audioTracks.begin();
        auto rightTrack = right.audioTracks.begin();
        for (; leftTrack != left.audioTracks.end(); ++leftTrack, ++rightTrack) {
            if ((*leftTrack)->mixer_track != (*rightTrack)->mixer_track
                || (*leftTrack)->output_track != (*rightTrack)->output_track)
                return false;
        }
        return true;
    }

    void UpdateSceneView(const MultiOutputConfig& newConfig)
    {
        if (!scene_view_ || !config_->videoConfig)
            return;

        auto videoConfig = newConfig.FindVideoConfig(*config_->videoConfig);
        if (!videoConfig || !videoConfig->outputScene)
            return;

        const auto& sceneName = *videoConfig->outputScene;
        OBSSourceAutoRelease next = obs_get_source_by_name(sceneName.c_str());
        if (!next) {
            blog(LOG_WARNING, TAG "Output scene '%s' is not found; keeping the current scene", sceneName.c_str());
            return;
        }

        if (!scene_view_->SetSource(next))
            blog(LOG_WARNING, TAG "Could not update output scene '%s'", sceneName.c_str());
    }

    OBSEncoder GetVideoEncoder() {
        auto config_id = config_->videoConfig.value_or(OBS_STREAMING_ENC_PLACEHOLDER);
        if (config_id == "" || config_id == OBS_STREAMING_ENC_PLACEHOLDER) {
            OBSOutputAutoRelease stream_output = obs_frontend_get_streaming_output();
            OBSEncoder enc = obs_output_get_video_encoder(stream_output);
            using_main_video_encoder_ = true;
            return enc.Get();
        } else if (config_id == OBS_RECORDING_ENC_PLACEHOLDER) {
            OBSOutputAutoRelease stream_output = obs_frontend_get_recording_output();
            OBSEncoder enc = obs_output_get_video_encoder(stream_output);
            using_main_video_encoder_ = true;
            return enc.Get();
        } else {
            OBSEncoderAutoRelease enc = obs_get_encoder_by_name(VideoEncoderName().c_str());
            if (!enc) {
                auto& global = GlobalMultiOutputConfig();
                auto videoConfig = global.FindVideoConfig(config_id);
                if (videoConfig) {
                    OBSDataAutoRelease settings = obs_data_create_from_json(videoConfig->encoderParams.dump().c_str());
                    enc = obs_video_encoder_create(videoConfig->encoderId.c_str(), VideoEncoderName().c_str(), settings, nullptr);
                    if (enc) {
                        auto wh = ParseResolution(videoConfig->resolution);
                        if (wh.has_value()) {
                            obs_encoder_set_gpu_scale_type(enc, obs_scale_type::OBS_SCALE_BICUBIC);
                            auto [w, h] = *wh;
                            obs_encoder_set_scaled_size(enc, w, h);
                        }
                        obs_encoder_set_frame_rate_divisor(enc, videoConfig->fpsDenumerator);
                    }
                } else {
                    assert(false && "No video encoder config found with specified id.");
                    blog(LOG_ERROR, TAG "Load video encoder config failed for %s. Sharing with main output.", config_->name.c_str());
                    config_->videoConfig = OBS_STREAMING_ENC_PLACEHOLDER;
                    return GetVideoEncoder();
                }
            }

            using_main_video_encoder_ = false;
            return enc.Get();
        }
    }

    OBSEncoder GetAudioEncoder(int trackIdx = 0, std::optional<int> mixerId = std::nullopt) {
        auto config_id = config_->audioConfig.value_or(OBS_STREAMING_ENC_PLACEHOLDER);
        if (config_id == "" || config_id == OBS_STREAMING_ENC_PLACEHOLDER) {
            OBSOutputAutoRelease stream_output = obs_frontend_get_streaming_output();
            OBSEncoder enc = obs_output_get_audio_encoder(stream_output, 0);
            using_main_audio_encoder_ = true;
            return enc.Get();
        } else if (config_id == OBS_RECORDING_ENC_PLACEHOLDER) {
            OBSOutputAutoRelease stream_output = obs_frontend_get_recording_output();
            OBSEncoder enc = obs_output_get_audio_encoder(stream_output, 0);
            using_main_audio_encoder_ = true;
            return enc.Get();
        } else {
            OBSEncoderAutoRelease enc = obs_get_encoder_by_name(AudioEncoderName(trackIdx).c_str());
            if (!enc) {
                auto& global = GlobalMultiOutputConfig();
                auto audioConfigId = *config_->audioConfig;
                auto audioConfig = global.FindAudioConfig(audioConfigId);
                if (audioConfig) {
                    OBSDataAutoRelease settings = obs_data_create_from_json(audioConfig->encoderParams.dump().c_str());

                    // If we were provided with a mixerId, override the audioConfig's mixerId with it
                    int defaultMixerId = audioConfig->mixerId;
                    if (auto overrideMixerId = mixerId) {
                        defaultMixerId = *overrideMixerId;
                    }

                    enc = obs_audio_encoder_create(audioConfig->encoderId.c_str(), AudioEncoderName(trackIdx).c_str(), settings, defaultMixerId, nullptr);
                } else {
                    assert(false && "No audio encoder config found with specified id.");
                    blog(LOG_ERROR, TAG "Load audio encoder config failed for %s. Sharing with main output.", config_->name.c_str());
                    config_->audioConfig = OBS_STREAMING_ENC_PLACEHOLDER;
                    return GetAudioEncoder();
                }
            }
            
            using_main_audio_encoder_ = false;
            return enc.Get();
        }
    }

    bool PrepareOutputEncoders()
    {
        if (!output_) {
            blog(LOG_ERROR, TAG "Prepare output encoder before output object is created.");
            return false;
        }

        auto& global = GlobalMultiOutputConfig();

        // main output
        OBSOutput mainOutput = obs_frontend_get_streaming_output();
        OBSOutput recordingOutput =  obs_frontend_get_recording_output();

        obs_output_release(mainOutput);
        obs_output_release(recordingOutput);

        OBSEncoder venc = GetVideoEncoder();
        OBSEncoder aenc = GetAudioEncoder();

        std::vector<std::tuple<int, OBSEncoder>> additionalTracks;
        if (auto audioConfigId = config_->audioConfig) {
            auto audioConfig = global.FindAudioConfig(*audioConfigId);

            if (!audioConfig) {
                blog(LOG_ERROR, TAG "Load audio encoder config failed for %s. Could not determine additional tracks.", config_->name.c_str());
            } else {
                additionalTracks.reserve(audioConfig->audioTracks.size());
                for (auto& track : audioConfig->audioTracks) {
                    OBSEncoder enc = GetAudioEncoder(track->output_track, track->mixer_track);
                    if (enc) {
                        // Record the output track index and the encoder for later when we set the encoders on the output
                        additionalTracks.push_back({ track->output_track, enc });
                    }
                }
            }
        }

        if (!aenc || !venc) {
            auto msgbox = new QMessageBox(QMessageBox::Icon::Critical, 
                obs_module_text("Notice.Title"), 
                obs_module_text("Notice.GetEncoder"),
                QMessageBox::StandardButton::Ok,
                this
                );
            msgbox->exec();
            return false;
        }

        // set_*_encoder() addrefs internally
        obs_output_set_audio_encoder(output_, aenc, 0);
        for (auto& track : additionalTracks) {
            obs_output_set_audio_encoder(output_, std::get<1>(track), std::get<0>(track));
        }
        obs_output_set_video_encoder(output_, venc);

        return true;
    }


    bool ReleaseOutput()
    {
        if (output_) {
            DisconnectSignals(output_);

            // failback: check if the output is still active and force stop it if necessary
            if (obs_output_active(output_))
                obs_output_force_stop(output_);

            // obs_output_destroy() clears the service's back-pointer
            obs_output_release(output_);
            output_ = nullptr;
        }

        // Safe here: the output that referenced the service is gone by now.
        ReleaseOutputService();

        // The output is gone; other targets may still hold the shared view.
        ReleaseOutputSceneView();

        return true;
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
            snprintf(strDuration, sizeof(strDuration), "%02d:%02d:%02d", (int)hh.count(), (int)mm.count(), (int)ss.count());

            char strFps[32] = { 0 };
            snprintf(strFps, sizeof(strFps), "%d FPS", static_cast<int>(std::round((new_frames - total_frames_) / interval)));

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

    void UpdateStatusDot(bool stopped = false) {
        if (!status_dot_)
            return;

        const char* color = "#888888"; // stopped / idle
        if (isReconnecting_ || isConnecting_)
            color = "#e6b800"; // connecting / reconnecting
        else if (!stopped && IsRunning())
            color = "#2ecc71"; // live
        else if (lastErrorCode_ != 0)
            color = "#e74c3c"; // last attempt failed

        status_dot_->setStyleSheet(QString("color: %1;").arg(color));
    }

    OBS_MULTI_RTMP_DEFINE_WEAK_THIS()

    template <typename Task>
    void RunInUIThread(Task&& task)
    {
        auto weakThis = GetWeakThis();
        if (!GetGlobalService().RunInUIThread([weakThis, task = std::forward<Task>(task)]() mutable {
            auto self = weakThis.Get();
            if (!self) {
                blog(LOG_WARNING, TAG "Skipping UI callback for a destroyed push widget.");
                return;
            }
            task();
        })) {
            blog(LOG_WARNING, TAG "Could not schedule push widget callback on the UI thread.");
        }
    }

public:
    PushWidgetImpl(const std::string& targetid, QWidget* parent = 0)
        : QWidget(parent)
        , targetid_(targetid)
    {
        QObject::setObjectName("push-widget");

        auto& global = GlobalMultiOutputConfig();
        config_ = global.FindTarget(targetid_);
        if (!config_)
            return;

        timer_ = new QTimer(this);
        timer_->setInterval(std::chrono::milliseconds(1000));
        QObject::connect(timer_, &QTimer::timeout, [this]() {
            UpdateStreamStatus();
        });

        auto layout = new QGridLayout(this);

        layout->addWidget(enabled_cb_ = new QCheckBox(this), 0, 0);
        enabled_cb_->setToolTip(obs_module_text("Target.Enabled"));
        QObject::connect(enabled_cb_, &QCheckBox::clicked, [this](bool checked) {
            SetEnabled(checked);
        });

        // Status dot hugs the title: both live in a zero-spacing row layout.
        auto titleWidget = new QWidget(this);
        auto titleLayout = new QHBoxLayout(titleWidget);
        titleLayout->setContentsMargins(0, 0, 0, 0);
        titleLayout->setSpacing(0);
        titleLayout->addWidget(status_dot_ = new QLabel(u8"●", this));
        titleLayout->addWidget(name_ = new QLabel(obs_module_text("NewStreaming"), this));
        titleLayout->addStretch(1);
        layout->addWidget(titleWidget, 0, 1);

        // Start/Stop shares the title row, pushed to the right edge.
        layout->addWidget(btn_ = new QPushButton(obs_module_text("Btn.Start"), this), 0, 2);
        QObject::connect(btn_, &QPushButton::clicked, [this]() {
            StartStop();
        });

        // Keep the button usable in narrow docks across OBS themes.
        btn_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
        btn_->setMinimumHeight(btn_->fontMetrics().height() + 8);
        {
            auto fm = btn_->fontMetrics();
            auto startW = fm.horizontalAdvance(QString::fromUtf8(obs_module_text("Btn.Start")));
            auto stopW = fm.horizontalAdvance(QString::fromUtf8(obs_module_text("Status.Stop")));
            btn_->setMinimumWidth((startW > stopW ? startW : stopW) + 24);
        }
        // Title takes the remaining space so the button stays compact.
        layout->setColumnStretch(1, 1);

        // Message text: keep it from soaking up spare height, otherwise the
        // item grows taller than its content.
        layout->addWidget(msg_ = new QLabel(u8"", this), 1, 0, 1, 3);
        msg_->setWordWrap(true);
        msg_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

        LoadConfig();
        UpdateStatusDot();

        auto startDesc = std::string(obs_module_text("Hotkey.StartTarget")) + " - " + config_->name;
        startHotkeyId_ = obs_hotkey_register_frontend(
            ("obs-multi-rtmp.start." + targetid_).c_str(),
            startDesc.c_str(),
            [](void* data, obs_hotkey_id, obs_hotkey_t*, bool pressed) {
                if (!pressed)
                    return;
                static_cast<PushWidgetImpl*>(data)->StartStreaming();
            },
            this
        );

        auto stopDesc = std::string(obs_module_text("Hotkey.StopTarget")) + " - " + config_->name;
        stopHotkeyId_ = obs_hotkey_register_frontend(
            ("obs-multi-rtmp.stop." + targetid_).c_str(),
            stopDesc.c_str(),
            [](void* data, obs_hotkey_id, obs_hotkey_t*, bool pressed) {
                if (!pressed)
                    return;
                static_cast<PushWidgetImpl*>(data)->StopStreaming();
            },
            this
        );
    }

    ~PushWidgetImpl()
    {
        if (startHotkeyId_ != OBS_INVALID_HOTKEY_ID)
            obs_hotkey_unregister(startHotkeyId_);
        if (stopHotkeyId_ != OBS_INVALID_HOTKEY_ID)
            obs_hotkey_unregister(stopHotkeyId_);

        ReleaseOutput();
    }

    // QLabel with word wrap reports a far too tall sizeHint, which makes the
    // list item reserve empty space below the message. Ask the layout for the
    // height it actually needs at the current width instead.
    int heightForWidth(int w) const override
    {
        auto lay = layout();
        if (!lay)
            return QWidget::heightForWidth(w);
        return lay->hasHeightForWidth() ? lay->heightForWidth(w) : lay->sizeHint().height();
    }

    QSize sizeHint() const override
    {
        auto lay = layout();
        if (!lay)
            return QWidget::sizeHint();

        auto hint = lay->sizeHint();
        hint.setHeight(heightForWidth(width() > 0 ? width() : hint.width()));
        return hint;
    }


    void StartStreaming() override {
        bool expected = false;
        if (IsBusy() || !isStarting_.compare_exchange_strong(expected, true))
            return;

        auto resetStarting = finally([this] { isStarting_ = false; });

        if (!config_->enabled) {
            SetMsg(obs_module_text("Status.Disabled"));
            return;
        }

        // recreate output
        ReleaseOutput();

        if (output_ == nullptr)
        {
            OBSDataAutoRelease output_settings = obs_data_create_from_json(config_->outputParam.dump().c_str());

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

        isBusy_ = true;
        if (!obs_output_start(output_))
        {
            isBusy_ = false;
            ReleaseOutput();
            SetMsg(obs_module_text("Error.StartOutput"));
            return;
        }

        // Start is asynchronous: keep the flag set until OnStarted()/OnStopped() clears it.
        resetStarting.Dismiss();
    }

    void StopStreaming() override {
        if (!IsBusy())
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
            if (!IsBusy() && config_->syncStart) {
                StartStop();
            }
        } else if (ev == obs_frontend_event::OBS_FRONTEND_EVENT_STREAMING_STOPPING) {
            if (IsBusy() && config_->syncStop) {
                StartStop();
            }
        }
    }

    void LoadConfig()
    {
        name_->setText(QString::fromUtf8(config_->name));
        if (enabled_cb_)
            enabled_cb_->setChecked(config_->enabled);
    }

    void UpdateConfig(const MultiOutputConfig& newConfig, GlobalConfigUpdateContext& context) override
    {
        auto newTarget = newConfig.FindTarget(targetid_);
        if (!newTarget)
            return;

        auto oldTarget = config_;
        auto& oldConfig = GlobalMultiOutputConfig();
        const bool busy = IsBusy();
        const auto targetName = QString::fromUtf8(newTarget->name.c_str());

        if (oldTarget->protocol != newTarget->protocol || oldTarget->serviceParam != newTarget->serviceParam) {
            context.notices.push_back(QString::fromUtf8(obs_module_text("Notice.ServiceChanged"))
                .arg(targetName).toUtf8().toStdString());
        }

        const bool videoIdChanged = oldTarget->videoConfig != newTarget->videoConfig;
        const bool audioIdChanged = oldTarget->audioConfig != newTarget->audioConfig;
        if ((videoIdChanged || audioIdChanged) && busy) {
            context.notices.push_back(QString::fromUtf8(obs_module_text("Notice.EncoderConfigChanged"))
                .arg(targetName).toUtf8().toStdString());
        }

        if (!videoIdChanged && newTarget->videoConfig) {
            const auto& configId = *newTarget->videoConfig;
            auto oldVideo = oldConfig.FindVideoConfig(configId);
            auto newVideo = newConfig.FindVideoConfig(configId);
            if (oldVideo && newVideo && !VideoConfigEqual(*oldVideo, *newVideo)) {
                OBSEncoderAutoRelease encoder = obs_get_encoder_by_name(("multi-rtmp-venc" + configId).c_str());
                if (encoder) {
                    if (context.updatedVideoConfigIds.insert(configId).second)
                        UpdateEncoderSettings(encoder, oldVideo->encoderParams, newVideo->encoderParams, "video encoder");
                    if (busy) {
                        context.notices.push_back(QString::fromUtf8(obs_module_text("Notice.EncoderParamsChanged"))
                            .arg(targetName).toUtf8().toStdString());
                    }
                }
            }
        }

        if (!audioIdChanged && newTarget->audioConfig) {
            const auto& configId = *newTarget->audioConfig;
            auto oldAudio = oldConfig.FindAudioConfig(configId);
            auto newAudio = newConfig.FindAudioConfig(configId);
            if (oldAudio && newAudio && !AudioConfigEqual(*oldAudio, *newAudio)) {
                std::unordered_set<int> trackIndices{ 0 };
                for (const auto& track : oldAudio->audioTracks)
                    trackIndices.insert(track->output_track);
                for (const auto& track : newAudio->audioTracks)
                    trackIndices.insert(track->output_track);

                bool hasEncoder = false;
                for (auto trackIndex : trackIndices) {
                    auto encoderName = "multi-rtmp-aenc" + configId + "-track-idx-" + std::to_string(trackIndex);
                    OBSEncoderAutoRelease encoder = obs_get_encoder_by_name(encoderName.c_str());
                    if (!encoder)
                        continue;
                    hasEncoder = true;
                    if (context.updatedAudioEncoderNames.insert(encoderName).second)
                        UpdateEncoderSettings(encoder, oldAudio->encoderParams, newAudio->encoderParams, "audio encoder");
                }
                if (hasEncoder && busy) {
                    context.notices.push_back(QString::fromUtf8(obs_module_text("Notice.EncoderParamsChanged"))
                        .arg(targetName).toUtf8().toStdString());
                }
            }
        }

        config_ = std::move(newTarget);
        LoadConfig();
        UpdateStatusDot();
        // A running output keeps its old encoder when the config id changed;
        // pointing its scene view at the new config's scene would mismatch.
        if (!videoIdChanged)
            UpdateSceneView(newConfig);
    }

    void ResetInfo()
    {
        total_frames_ = 0;
        total_bytes_ = 0;
        last_info_time_ = clock::now();
        msg_->setText("");
    }

    bool IsRunning() override
    {
        return output_ != nullptr && obs_output_active(output_);
    }

    bool IsBusy() override
    {
        return isBusy_;
    }

    const std::string& GetTargetId() const override
    {
        return targetid_;
    }

    OutputTargetConfigPtr GetTargetConfig() const override
    {
        return config_;
    }

    bool GetEnabled() override { return config_->enabled; }

    void SetEnabled(bool enabled) override
    {
        config_->enabled = enabled;
        if (enabled_cb_ && enabled_cb_->isChecked() != enabled)
            enabled_cb_->setChecked(enabled);
        if (!enabled && IsRunning())
            obs_output_force_stop(output_);
        SaveMultiOutputConfig();
        UpdateStatusDot();
    }

    void StartStop()
    {
        if (IsBusy())
        {
            StopStreaming();
            return;
        }

        StartStreaming();
    }

    void Stop()
    {
        if (IsBusy())
        {
            obs_output_force_stop(output_);
        }
    }

    void SetMsg(QString msg)
    {
        msg_->setText(msg);
        msg_->setToolTip(msg);
    }

    // obs logical
    void OnStarting() override
    {
        RunInUIThread([this]() {
            begin_time_ = clock::now();
            isConnecting_ = true;
            isReconnecting_ = false;
            lastErrorCode_ = 0;
            btn_->setText(obs_module_text("Status.Stop"));
            btn_->setEnabled(true);
            SetMsg(obs_module_text("Status.Connecting"));
            UpdateStatusDot();
        });
    }

    void OnStarted() override
    {
        RunInUIThread([this]() {
            isStarting_ = false;
            isConnecting_ = false;
            isReconnecting_ = false;
            btn_->setText(obs_module_text("Status.Stop"));
            btn_->setEnabled(true);
            SetMsg(obs_module_text("Status.Streaming"));

            ResetInfo();
            timer_->start();
            UpdateStatusDot();
        });
    }

    void OnReconnect() override
    {
        RunInUIThread([this]() {
            timer_->stop();
            isReconnecting_ = true;

            btn_->setText(obs_module_text("Status.Stop"));
            btn_->setEnabled(true);
            SetMsg(obs_module_text("Status.Reconnecting"));
            UpdateStatusDot();
        });
    }

    void OnReconnected() override
    {
        RunInUIThread([this]() {
            isReconnecting_ = false;
            btn_->setText(obs_module_text("Status.Stop"));
            btn_->setEnabled(true);
            SetMsg(obs_module_text("Status.Streaming"));

            ResetInfo();
            timer_->start();
            UpdateStatusDot();
        });
    }

    void OnStopping() override
    {
        RunInUIThread([this]() {
            timer_->stop();

            btn_->setText(obs_module_text("Status.Stop"));
            btn_->setEnabled(true);
            SetMsg(obs_module_text("Status.Stopping"));
        });
    }

    void OnStopped(int code) override
    {
        isBusy_ = false;
        RunInUIThread([this, code]() {
            ReleaseOutputSceneView();

            isStarting_ = false;
            isConnecting_ = false;
            isReconnecting_ = false;
            lastErrorCode_ = code;
            ResetInfo();
            timer_->stop();

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

            UpdateStatusDot(true);
        });
    }
};

PushWidget* createPushWidget(const std::string& targetid, QWidget* parent) {
    return new PushWidgetImpl(targetid, parent);
}
