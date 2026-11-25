#include "pch.h"
#include "helpers.h"
#include <regex>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <tuple>
#include <unordered_map>
#include <QCoreApplication>
#include <QPointer>
#include <QMetaObject>
#include "push-widget.h"
#include "encoder-source.h"
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
    if (!updatedSettings)
        return;

    // obs_encoder_update() applies on top of the live settings; the merged
    // json already carries defaults for removed keys, so there is no need to
    // clear the live object (an encoder thread could read it half-empty).
    obs_encoder_update(encoder, updatedSettings);
}

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
    // Locale key for the last encoder-resolution failure (null when ok).
    // Set by GetVideoEncoder()/GetAudioEncoder(), consumed by
    // PrepareOutputEncoders() for an actionable message.
    const char* lastEncoderError_ = nullptr;
    std::atomic_bool isBusy_ = false;

    // Registered under the stable target id (not the display name) so key
    // bindings survive renames. Their description label is set once at
    // registration time and won't update if the target is renamed later.
    obs_hotkey_id startHotkeyId_ = OBS_INVALID_HOTKEY_ID;
    obs_hotkey_id stopHotkeyId_ = OBS_INVALID_HOTKEY_ID;

    enum class RestreamState { Stopped, Preparing, Connecting, Streaming, Stopping };
    RestreamState restreamState_ = RestreamState::Stopped;
    std::atomic<uint64_t> outputGeneration_{0};

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
            auto audioConfig = FindById(GlobalMultiOutputConfig().audioConfig, config_->audioConfig.value_or(""));
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

    // Snapshot of the OBS profile for Copy modes. Reads only public
    // frontend/profile APIs; never logs keys or tokens.
    static ObsProfileView ReadObsProfileView()
    {
        ObsProfileView view;
        config_t *profile = obs_frontend_get_profile_config();
        if (!profile)
            return view;
        auto getStr = [&](const char *section, const char *key) -> std::string {
            const char *v = config_get_string(profile, section, key);
            return v ? v : "";
        };
        view.outputMode = getStr("Output", "Mode");
        view.simpleStreamVideo = getStr("SimpleOutput", "StreamEncoder");
        view.simpleStreamAudio = getStr("SimpleOutput", "StreamAudioEncoder");
        view.simpleRecVideo = getStr("SimpleOutput", "RecEncoder");
        view.simpleRecAudio = getStr("SimpleOutput", "RecAudioEncoder");
        view.advStreamVideo = getStr("AdvOut", "Encoder");
        view.advStreamAudio = getStr("AdvOut", "AudioEncoder");
        view.advRecVideo = getStr("AdvOut", "RecEncoder");
        view.advRecAudio = getStr("AdvOut", "RecAudioEncoder");
        view.recUsesStreamEncoder = view.advRecVideo.empty() || view.advRecVideo == "none";
        return view;
    }

    static std::string CurrentProfileDir()
    {
        std::string dir;
        char *path = obs_frontend_get_current_profile_path();
        if (path) {
            dir = path;
            bfree(path);
        }
        return dir;
    }

    // Copy the *live* encoder's id+settings when it exists (exact snapshot),
    // else fall back to the profile files. Returns nullopt when neither
    // yields a usable spec; sets lastEncoderError_ to an actionable key.
    std::optional<CopiedEncoderSpec> ResolveCopySpec(EncoderSourceMode mode, bool isVideo)
    {
        // Prefer the live instance for exact parity when OBS currently has
        // one; this also covers hardware encoders whose Simple short names
        // cannot be mapped safely from the profile alone.
        {
            OBSOutputAutoRelease src = (mode == EncoderSourceMode::CopyStreaming)
                                           ? obs_frontend_get_streaming_output()
                                           : obs_frontend_get_recording_output();
            obs_encoder_t *live = nullptr;
            if (src) {
                live = isVideo ? obs_output_get_video_encoder(src)
                               : obs_output_get_audio_encoder(src, 0);
            }
            if (live) {
                const char *id = obs_encoder_get_id(live);
                if (id && *id) {
                    const obs_data_t *borrowed = obs_encoder_get_settings(live);
                    // Borrowed settings; copy the JSON text so the new
                    // encoder owns its settings (snapshot-on-start).
                    std::string json = "{}";
                    if (borrowed) {
                        const char *text = obs_data_get_json((obs_data_t *)borrowed);
                        if (text && *text)
                            json = text;
                    }
                    return CopiedEncoderSpec{std::string(id), json};
                }
            }
        }
        EncoderResolveError err = EncoderResolveError::None;
        auto spec = SnapshotCopyEncoder(ReadObsProfileView(), CurrentProfileDir(), mode, isVideo, &err);
        if (!spec && err == EncoderResolveError::CopyUnresolvable)
            lastEncoderError_ = "Error.CopyEncoderUnresolvable";
        else if (!spec)
            lastEncoderError_ = EncoderResolveErrorKey(err);
        return spec;
    }

    // Borrowed live OBS encoder for a Share mode, or null when the OBS
    // source output/encoder is not ready. Never creates or mutates.
    static obs_encoder_t *BorrowSharedVideoEncoder(EncoderSourceMode mode)
    {
        OBSOutputAutoRelease src = (mode == EncoderSourceMode::ShareStreaming)
                                       ? obs_frontend_get_streaming_output()
                                       : obs_frontend_get_recording_output();
        if (!src)
            return nullptr;
        return obs_output_get_video_encoder(src);
    }

    static obs_encoder_t *BorrowSharedAudioEncoder(EncoderSourceMode mode, size_t slot)
    {
        OBSOutputAutoRelease src = (mode == EncoderSourceMode::ShareStreaming)
                                       ? obs_frontend_get_streaming_output()
                                       : obs_frontend_get_recording_output();
        if (!src)
            return nullptr;
        return obs_output_get_audio_encoder(src, slot);
    }

    // Resolves the configured video encoder and returns an OWNING reference
    // (libobs output setters take their own reference from the raw pointer,
    // so callers must pass .Get() without an extra get_ref).
    // Never silently substitutes another encoder: a missing custom config,
    // an invalid resolution/FPS divisor, a failed creation, or an
    // unavailable shared OBS encoder yields null plus lastEncoderError_.
    OBSEncoder GetVideoEncoder() {
        lastEncoderError_ = nullptr;
        auto config_id = config_->videoConfig.value_or(OBS_STREAMING_ENC_PLACEHOLDER);
        if (config_id == "" || config_id == OBS_STREAMING_ENC_PLACEHOLDER) {
            OBSEncoder enc = BorrowSharedVideoEncoder(EncoderSourceMode::ShareStreaming);
            if (!enc) {
                blog(LOG_WARNING, TAG "Shared OBS streaming encoder unavailable for '%s'. "
                                      "Start OBS streaming first, or choose Copy/Custom.",
                     config_->name.c_str());
                lastEncoderError_ = "Error.SharedEncoderUnavailable";
                return {};
            }
            using_main_video_encoder_ = true;
            return enc;
        } else if (config_id == OBS_RECORDING_ENC_PLACEHOLDER) {
            OBSEncoder enc = BorrowSharedVideoEncoder(EncoderSourceMode::ShareRecording);
            if (!enc) {
                blog(LOG_WARNING, TAG "Shared OBS recording encoder unavailable for '%s'. "
                                      "Start OBS recording first, or choose Copy/Custom.",
                     config_->name.c_str());
                lastEncoderError_ = "Error.SharedEncoderUnavailable";
                return {};
            }
            using_main_video_encoder_ = true;
            return enc;
        } else if (config_id == OBS_STREAMING_COPY_PLACEHOLDER ||
                   config_id == OBS_RECORDING_COPY_PLACEHOLDER) {
            // Copy OBS configured settings into an independent encoder.
            // Snapshot-on-start: OBS changes apply on the next start, never
            // hot-applied mid-stream. Works while OBS is idle.
            EncoderSourceMode mode = ResolveEncoderSourceMode(config_id);
            OBSEncoderAutoRelease existing = obs_get_encoder_by_name(VideoEncoderName().c_str());
            if (existing) {
                using_main_video_encoder_ = false;
                OBSEncoder owned = existing.Get();
                return owned;
            }
            auto spec = ResolveCopySpec(mode, true);
            if (!spec) {
                if (!lastEncoderError_)
                    lastEncoderError_ = "Error.CopyEncoderUnresolvable";
                blog(LOG_WARNING, TAG "Copy OBS video settings unresolvable for '%s' (mode=%s).",
                     config_->name.c_str(), config_id.c_str());
                return {};
            }
            OBSDataAutoRelease settings = obs_data_create_from_json(spec->settingsJson.c_str());
            OBSEncoderAutoRelease enc =
                obs_video_encoder_create(spec->encoderId.c_str(), VideoEncoderName().c_str(),
                                         settings, nullptr);
            if (!enc) {
                blog(LOG_ERROR, TAG "Failed to create copied video encoder '%s' for '%s'.",
                     spec->encoderId.c_str(), config_->name.c_str());
                lastEncoderError_ = "Error.CreateEncoder";
                return {};
            }
            blog(LOG_INFO, TAG "Copy video settings for '%s': encoder=%s (snapshot-on-start).",
                 config_->name.c_str(), spec->encoderId.c_str());
            using_main_video_encoder_ = false;
            OBSEncoder owned = enc.Get();
            return owned;
        } else {
            OBSEncoderAutoRelease enc = obs_get_encoder_by_name(VideoEncoderName().c_str());
            if (!enc) {
                auto& global = GlobalMultiOutputConfig();
                auto videoConfig = FindById(global.videoConfig, config_id);
                if (!videoConfig) {
                    blog(LOG_ERROR, TAG "Video encoder config '%s' for '%s' not found.",
                         config_id.c_str(), config_->name.c_str());
                    lastEncoderError_ = "Error.MissingVideoConfig";
                    return {};
                }
                if (videoConfig->resolution.has_value() &&
                    !ParseResolution(videoConfig->resolution).has_value()) {
                    blog(LOG_ERROR, TAG "Invalid video resolution '%s' for '%s'.",
                         videoConfig->resolution->c_str(), config_->name.c_str());
                    lastEncoderError_ = "Error.InvalidVideoConfig";
                    return {};
                }
                if (videoConfig->fpsDenumerator <= 0) {
                    blog(LOG_ERROR, TAG "Invalid video FPS divisor for '%s'.",
                         config_->name.c_str());
                    lastEncoderError_ = "Error.InvalidVideoConfig";
                    return {};
                }
                OBSDataAutoRelease settings = obs_data_create_from_json(videoConfig->encoderParams.dump().c_str());
                enc = obs_video_encoder_create(videoConfig->encoderId.c_str(), VideoEncoderName().c_str(), settings, nullptr);
                if (!enc) {
                    blog(LOG_ERROR, TAG "Failed to create video encoder '%s' for '%s'.",
                         videoConfig->encoderId.c_str(), config_->name.c_str());
                    lastEncoderError_ = "Error.CreateEncoder";
                    return {};
                }
                auto wh = ParseResolution(videoConfig->resolution);
                if (wh.has_value()) {
                    obs_encoder_set_gpu_scale_type(enc, obs_scale_type::OBS_SCALE_BICUBIC);
                    auto [w, h] = *wh;
                    obs_encoder_set_scaled_size(enc, w, h);
                }
                obs_encoder_set_frame_rate_divisor(enc, videoConfig->fpsDenumerator);
            }

            using_main_video_encoder_ = false;
            OBSEncoder owned = enc.Get();
            return owned;
        }
    }

    // Same ownership contract as GetVideoEncoder(). Mixer and output-track
    // indices are validated before creation; out-of-range values yield null
    // plus lastEncoderError_ instead of a misbound encoder.
    OBSEncoder GetAudioEncoder(int trackIdx = 0, std::optional<int> mixerId = std::nullopt) {
        lastEncoderError_ = nullptr;
        auto config_id = config_->audioConfig.value_or(OBS_STREAMING_ENC_PLACEHOLDER);
        if (config_id == "" || config_id == OBS_STREAMING_ENC_PLACEHOLDER) {
            OBSEncoder enc = BorrowSharedAudioEncoder(EncoderSourceMode::ShareStreaming, 0);
            if (!enc) {
                blog(LOG_WARNING, TAG "Shared OBS streaming audio encoder unavailable for '%s'. "
                                      "Start OBS streaming first, or choose Copy/Custom.",
                     config_->name.c_str());
                lastEncoderError_ = "Error.SharedEncoderUnavailable";
                return {};
            }
            using_main_audio_encoder_ = true;
            return enc;
        } else if (config_id == OBS_RECORDING_ENC_PLACEHOLDER) {
            // Recording outputs may carry multiple tracks; slot 0 is the
            // primary track. A missing slot 0 is a track problem, not a
            // generic unavailability, so it gets its own message.
            OBSEncoder enc = BorrowSharedAudioEncoder(EncoderSourceMode::ShareRecording, 0);
            if (!enc) {
                OBSOutputAutoRelease rec = obs_frontend_get_recording_output();
                if (!rec) {
                    blog(LOG_WARNING, TAG "Shared OBS recording audio unavailable for '%s': "
                                          "recording output inactive.",
                         config_->name.c_str());
                    lastEncoderError_ = "Error.SharedEncoderUnavailable";
                } else {
                    blog(LOG_WARNING, TAG "Shared OBS recording audio track 1 unavailable for '%s'.",
                         config_->name.c_str());
                    lastEncoderError_ = "Error.MissingAudioTrack";
                }
                return {};
            }
            using_main_audio_encoder_ = true;
            return enc;
        } else if (config_id == OBS_STREAMING_COPY_PLACEHOLDER ||
                   config_id == OBS_RECORDING_COPY_PLACEHOLDER) {
            EncoderSourceMode mode = ResolveEncoderSourceMode(config_id);
            if (trackIdx < 0 || trackIdx >= 6) {
                lastEncoderError_ = "Error.InvalidAudioConfig";
                return {};
            }
            OBSEncoderAutoRelease existing = obs_get_encoder_by_name(AudioEncoderName(trackIdx).c_str());
            if (existing) {
                using_main_audio_encoder_ = false;
                OBSEncoder owned = existing.Get();
                return owned;
            }
            auto spec = ResolveCopySpec(mode, false);
            if (!spec) {
                if (!lastEncoderError_)
                    lastEncoderError_ = "Error.CopyEncoderUnresolvable";
                return {};
            }
            // Copied audio encoders attach to the main mix (mixer 0); the
            // track slot is resolved by the caller via trackIdx.
            OBSDataAutoRelease settings = obs_data_create_from_json(spec->settingsJson.c_str());
            OBSEncoderAutoRelease enc = obs_audio_encoder_create(
                spec->encoderId.c_str(), AudioEncoderName(trackIdx).c_str(), settings, 0, nullptr);
            if (!enc) {
                blog(LOG_ERROR, TAG "Failed to create copied audio encoder '%s' for '%s'.",
                     spec->encoderId.c_str(), config_->name.c_str());
                lastEncoderError_ = "Error.CreateEncoder";
                return {};
            }
            blog(LOG_INFO, TAG "Copy audio settings for '%s': encoder=%s (snapshot-on-start).",
                 config_->name.c_str(), spec->encoderId.c_str());
            using_main_audio_encoder_ = false;
            OBSEncoder owned = enc.Get();
            return owned;
        } else {
            OBSEncoderAutoRelease enc = obs_get_encoder_by_name(AudioEncoderName(trackIdx).c_str());
            if (!enc) {
                auto& global = GlobalMultiOutputConfig();
                auto audioConfigId = *config_->audioConfig;
                auto audioConfig = FindById(global.audioConfig, audioConfigId);
                if (!audioConfig) {
                    blog(LOG_ERROR, TAG "Audio encoder config '%s' for '%s' not found.",
                         audioConfigId.c_str(), config_->name.c_str());
                    lastEncoderError_ = "Error.MissingAudioConfig";
                    return {};
                }
                // If we were provided with a mixerId, override the audioConfig's mixerId with it
                int defaultMixerId = audioConfig->mixerId;
                if (auto overrideMixerId = mixerId) {
                    defaultMixerId = *overrideMixerId;
                }
                if (defaultMixerId < 0 || defaultMixerId >= 6 || trackIdx < 0 || trackIdx >= 6) {
                    blog(LOG_ERROR, TAG "Invalid audio mixer/track index for '%s'.",
                         config_->name.c_str());
                    lastEncoderError_ = "Error.InvalidAudioConfig";
                    return {};
                }

                OBSDataAutoRelease settings = obs_data_create_from_json(audioConfig->encoderParams.dump().c_str());
                enc = obs_audio_encoder_create(audioConfig->encoderId.c_str(), AudioEncoderName(trackIdx).c_str(), settings, defaultMixerId, nullptr);
                if (!enc) {
                    blog(LOG_ERROR, TAG "Failed to create audio encoder '%s' for '%s'.",
                         audioConfig->encoderId.c_str(), config_->name.c_str());
                    lastEncoderError_ = "Error.CreateEncoder";
                    return {};
                }
            }

            using_main_audio_encoder_ = false;
            OBSEncoder owned = enc.Get();
            return owned;
        }
    }

    bool PrepareOutputEncoders()
    {
        if (!output_) {
            blog(LOG_ERROR, TAG "Prepare output encoder before output object is created.");
            return false;
        }

        ReleaseOutputEncoder();

        auto& global = GlobalMultiOutputConfig();

        OBSEncoder venc = GetVideoEncoder();
        const char* videoError = lastEncoderError_;
        OBSEncoder aenc = GetAudioEncoder();
        const char* audioError = lastEncoderError_;

        std::vector<std::tuple<int, OBSEncoder>> additionalTracks;
        const char* extraTrackError = nullptr;
        if (auto audioConfigId = config_->audioConfig) {
            auto audioConfig = FindById(global.audioConfig, *audioConfigId);

            if (!audioConfig) {
                blog(LOG_ERROR, TAG "Load audio encoder config failed for %s. Could not determine additional tracks.", config_->name.c_str());
            } else {
                additionalTracks.reserve(audioConfig->audioTracks.size());
                for (auto& track : audioConfig->audioTracks) {
                    OBSEncoder enc = GetAudioEncoder(track->output_track, track->mixer_track);
                    if (enc) {
                        // Record the output track index and the encoder for later when we set the encoders on the output
                        additionalTracks.emplace_back(track->output_track, enc);
                    } else if (!extraTrackError) {
                        // A configured extra track must not be dropped
                        // silently; fail with its specific cause below.
                        extraTrackError = lastEncoderError_ ? lastEncoderError_ : "Error.CreateEncoder";
                    }
                }
            }
        }

        if (!aenc || !venc || extraTrackError) {
            // Never substitute another encoder here. Either the user picked
            // a shared OBS encoder that is unavailable (start OBS streaming
            // or recording first), or the saved custom config is missing or
            // invalid: both are actionable, so report the specific cause.
            ReleaseOutputEncoder();

            const char* key = extraTrackError ? extraTrackError : (!venc ? videoError : audioError);
            if (!key)
                key = "Notice.GetEncoder";
            auto msgbox = new QMessageBox(QMessageBox::Icon::Critical,
                obs_module_text("Notice.Title"),
                obs_module_text(key),
                QMessageBox::StandardButton::Ok,
                this
                );
            msgbox->exec();
            return false;
        }

        // The setters take their own reference; the owning wrappers below
        // release ours at scope end. No extra get_ref: that over-counted by
        // one per encoder on every start.
        obs_output_set_audio_encoder(output_, aenc.Get(), 0);
        for (auto& track : additionalTracks) {
            auto trackIdx = std::get<0>(track);
            auto enc = std::get<1>(track);
            obs_output_set_audio_encoder(output_, enc.Get(), trackIdx);
        }
        obs_output_set_video_encoder(output_, venc.Get());

        return true;
    }


    // libobs can address 6 audio encoder slots per output (see
    // MAX_OUTPUT_AUDIO_ENCODERS in libobs/obs-output.h).
    static constexpr size_t kMaxOutputAudioEncoders = 6;

    bool ReleaseOutputEncoder()
    {
        if (!output_)
            return true;
        else if (obs_output_active(output_) == false)
        {
            // The setters already release the output's own reference when
            // clearing a slot, so no manual release follows: the getters
            // below return borrowed pointers and releasing them would
            // consume someone else's reference.
            auto venc = obs_output_get_video_encoder(output_);
            if (venc)
                obs_output_set_video_encoder(output_, nullptr);

            for (size_t i = 0; i < kMaxOutputAudioEncoders; ++i) {
                auto aenc = obs_output_get_audio_encoder(output_, i);
                if (aenc)
                    obs_output_set_audio_encoder(output_, nullptr, i);
            }

            return true;
        }
        else {
            blog(LOG_ERROR, TAG "Release output while it is active.");
            return false;
        }
    }


    bool ReleaseObsOutput()
    {
        ++outputGeneration_;
        restreamState_ = RestreamState::Stopped;
        if (timer_) timer_->stop();
        if (btn_) {
            btn_->setText(obs_module_text("Btn.Start"));
            btn_->setEnabled(true);
        }
        if (!output_)
            return true;

        if (obs_output_active(output_)) {
            // Asynchronous stop: ask libobs to stop but keep every local
            // reference (signals, encoders, service, scene view) until the
            // terminal OnStopped() runs this same release to completion.
            // Dropping references now would pull objects out from under the
            // streaming thread and leak the service/scene cleanup below.
            obs_output_force_stop(output_);
            if (obs_output_active(output_))
                return true;
            // The stop completed synchronously; fall through to the full
            // release while the output is verifiably idle.
        }

        DisconnectSignals(output_);
        bool ret = ReleaseOutputEncoder();

        // obs_output_destroy() clears the service's weak back-pointer.
        obs_output_release(output_);
        output_ = nullptr;
        ReleaseOutputService();

        ret = ReleaseOutputSceneView() && ret;

        return ret;
    }

    // Synchronous variant for owner teardown (target deletion, profile
    // change, OBS exit): teardown cannot rely on a future OnStopped()
    // callback because the owner is going away, so when a stop is still in
    // flight here the signals are disconnected first and the final output
    // reference is dropped; libobs stops synchronously inside destroy,
    // after which the service and scene view are released exactly once. No
    // OnStopped() can arrive afterwards (signals disconnected).
    void ForceReleaseObsOutput()
    {
        ReleaseObsOutput();
        if (!output_)
            return;
        DisconnectSignals(output_);
        obs_output_release(output_);
        output_ = nullptr;
        ReleaseOutputService();
        ReleaseOutputSceneView();
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

    void UpdateStatusDot() {
        if (!status_dot_)
            return;

        const char* color = "#888888"; // stopped / idle
        if (isReconnecting_ || isConnecting_)
            color = "#e6b800"; // connecting / reconnecting
        else if (IsRunning())
            color = "#2ecc71"; // live
        else if (lastErrorCode_ != 0)
            color = "#e74c3c"; // last attempt failed

        status_dot_->setStyleSheet(QString("color: %1;").arg(color));
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

        setLayout(layout);

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
        ForceReleaseObsOutput();
        if (startHotkeyId_ != OBS_INVALID_HOTKEY_ID)
            obs_hotkey_unregister(startHotkeyId_);
        if (stopHotkeyId_ != OBS_INVALID_HOTKEY_ID)
            obs_hotkey_unregister(stopHotkeyId_);
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

    static bool CodecInList(const char* list, const char* codec)
    {
        if (!list || !*list)
            return true; // No restriction declared by the output.
        if (!codec || !*codec)
            return true; // Unknown encoder id: creation will report it.
        std::string items(list);
        std::string want(codec);
        size_t pos = 0;
        while (pos <= items.size()) {
            size_t end = items.find(';', pos);
            if (end == std::string::npos)
                end = items.size();
            if (items.compare(pos, end - pos, want) == 0)
                return true;
            pos = end + 1;
        }
        return false;
    }

    // Side-effect-free local preflight for the AUTOMATIC endpoint creation
    // path: validates everything StartRestream() needs except the actual
    // start, without creating a remote LIVE. Returns a locale key on
    // failure, nullptr when the local configuration is usable. A temporary
    // output is created for the codec check and always destroyed here, so
    // StartRestream() still owns the real preparation. The manual Create
    // Endpoint button intentionally bypasses this: a LIVE may be prepared
    // before encoders exist.
    const char* ValidateLocalEncoderConfiguration()
    {
        std::string customVideoId;
        std::string sharedVideoCodec;
        std::string copyVideoId;
        // Video track: existence, values, scene, shared availability.
        {
            auto config_id = config_->videoConfig.value_or(OBS_STREAMING_ENC_PLACEHOLDER);
            EncoderSourceMode mode = ResolveEncoderSourceMode(config_id);
            if (mode == EncoderSourceMode::ShareStreaming ||
                mode == EncoderSourceMode::ShareRecording) {
                obs_encoder_t *live = BorrowSharedVideoEncoder(mode);
                if (!live)
                    return "Error.SharedEncoderUnavailable";
                const char *codec = obs_encoder_get_codec(live);
                if (codec && *codec)
                    sharedVideoCodec = codec;
            } else if (mode == EncoderSourceMode::CopyStreaming ||
                       mode == EncoderSourceMode::CopyRecording) {
                auto view = ReadObsProfileView();
                auto id = ResolveCopyVideoEncoderId(view, mode);
                if (!id || id->empty())
                    return "Error.CopyEncoderUnresolvable";
                copyVideoId = *id;
            } else {
                auto& global = GlobalMultiOutputConfig();
                auto videoConfig = FindById(global.videoConfig, config_id);
                if (!videoConfig)
                    return "Error.MissingVideoConfig";
                if (videoConfig->resolution.has_value() &&
                    !ParseResolution(videoConfig->resolution).has_value())
                    return "Error.InvalidVideoConfig";
                if (videoConfig->fpsDenumerator <= 0)
                    return "Error.InvalidVideoConfig";
                if (videoConfig->outputScene.has_value()) {
                    OBSSourceAutoRelease scene =
                        obs_get_source_by_name(videoConfig->outputScene->c_str());
                    if (!scene)
                        return "Error.SceneNotExist";
                }
                customVideoId = videoConfig->encoderId;
            }
        }
        // Audio track: existence, values, shared availability (track 0).
        std::string customAudioEncoder;
        std::string sharedAudioCodec;
        std::string copyAudioId;
        {
            auto config_id = config_->audioConfig.value_or(OBS_STREAMING_ENC_PLACEHOLDER);
            EncoderSourceMode mode = ResolveEncoderSourceMode(config_id);
            if (mode == EncoderSourceMode::ShareStreaming) {
                obs_encoder_t *live = BorrowSharedAudioEncoder(mode, 0);
                if (!live)
                    return "Error.SharedEncoderUnavailable";
                const char *codec = obs_encoder_get_codec(live);
                if (codec && *codec)
                    sharedAudioCodec = codec;
            } else if (mode == EncoderSourceMode::ShareRecording) {
                OBSOutputAutoRelease rec = obs_frontend_get_recording_output();
                if (!rec)
                    return "Error.SharedEncoderUnavailable";
                obs_encoder_t *live = obs_output_get_audio_encoder(rec, 0);
                if (!live)
                    return "Error.MissingAudioTrack";
                const char *codec = obs_encoder_get_codec(live);
                if (codec && *codec)
                    sharedAudioCodec = codec;
            } else if (mode == EncoderSourceMode::CopyStreaming ||
                       mode == EncoderSourceMode::CopyRecording) {
                auto view = ReadObsProfileView();
                auto id = ResolveCopyAudioEncoderId(view, mode);
                if (!id || id->empty())
                    return "Error.CopyEncoderUnresolvable";
                copyAudioId = *id;
            } else {
                auto& global = GlobalMultiOutputConfig();
                auto audioConfig = FindById(global.audioConfig, config_id);
                if (!audioConfig)
                    return "Error.MissingAudioConfig";
                if (audioConfig->mixerId < 0 || audioConfig->mixerId >= 6)
                    return "Error.InvalidAudioConfig";
                for (auto& track : audioConfig->audioTracks) {
                    if (!track || track->output_track < 1 || track->output_track >= 6 ||
                        track->mixer_track < 0 || track->mixer_track >= 6)
                        return "Error.InvalidAudioConfig";
                }
                customAudioEncoder = audioConfig->encoderId;
            }
        }
        // The encoder IDs must be registered at all: libobs reports a
        // null codec for unknown IDs, and an unregistered encoder could
        // otherwise pass every check above, mint a remote LIVE, and only
        // fail when the real encoder is created afterwards.
        auto codecOf = [](const std::string &id) -> const char * {
            if (id.empty())
                return "";
            return obs_get_encoder_codec(id.c_str());
        };
        const char* videoCodec = codecOf(customVideoId.empty() ? copyVideoId : customVideoId);
        const char* audioCodec = codecOf(customAudioEncoder.empty() ? copyAudioId : customAudioEncoder);
        if ((!customVideoId.empty() || !copyVideoId.empty()) && (!videoCodec || !*videoCodec))
            return "Error.CreateEncoder";
        if ((!customAudioEncoder.empty() || !copyAudioId.empty()) && (!audioCodec || !*audioCodec))
            return "Error.CreateEncoder";
        // Codec support against the output for every mode, including shared
        // instances read from the live encoder. A temporary output is
        // created for the check and always destroyed here.
        if (!CreateObsOutput())
            return "Error.StartOutput";
        auto cleanup = finally([this] { ReleaseObsOutput(); });
        auto checkVideo = [&](const char *codec) -> const char * {
            if (!codec || !*codec)
                return nullptr;
            if (!CodecSupportedByOutput(obs_output_get_supported_video_codecs(output_) ?
                                            obs_output_get_supported_video_codecs(output_) : "",
                                        codec))
                return "Error.UnsupportedVideoCodec";
            return nullptr;
        };
        auto checkAudio = [&](const char *codec) -> const char * {
            if (!codec || !*codec)
                return nullptr;
            if (!CodecSupportedByOutput(obs_output_get_supported_audio_codecs(output_) ?
                                            obs_output_get_supported_audio_codecs(output_) : "",
                                        codec))
                return "Error.UnsupportedAudioCodec";
            return nullptr;
        };
        if (!sharedVideoCodec.empty()) {
            if (const char *err = checkVideo(sharedVideoCodec.c_str()))
                return err;
        }
        if (!sharedAudioCodec.empty()) {
            if (const char *err = checkAudio(sharedAudioCodec.c_str()))
                return err;
        }
        if (!customVideoId.empty() || !copyVideoId.empty()) {
            if (const char *err = checkVideo(videoCodec))
                return err;
        }
        if (!customAudioEncoder.empty() || !copyAudioId.empty()) {
            if (const char *err = checkAudio(audioCodec))
                return err;
        }
        return nullptr;
    }

    void StartStreaming() override {
        if (!config_->enabled) {
            SetMsg(obs_module_text("Status.Disabled"));
            return;
        }
        bool expected = false;
        if (IsBusy() || !isStarting_.compare_exchange_strong(expected, true))
            return;
        auto resetStarting = finally([this] { isStarting_ = false; });

        if (restreamState_ != RestreamState::Stopped) return;
        if (const char* preflight = ValidateLocalEncoderConfiguration()) {
            blog(LOG_WARNING, TAG "Local encoder preflight failed for '%s' (%s)",
                 config_->name.c_str(), preflight);
            SetMsg(obs_module_text(preflight));
            return;
        }
        if (StartRestream())
            resetStarting.Dismiss();
    }

    // Returns true when an async OBS start is in flight (OnStarted/
    // OnStopped will clear the starting guard); false otherwise.
    bool StartRestream() {
        auto clearStarting = finally([this] { isStarting_ = false; });
        if (IsRunning())
            return false;

        if (restreamState_ != RestreamState::Stopped) return false;

        ReleaseObsOutput();
        restreamState_ = RestreamState::Preparing;

        if (!CreateObsOutput()) {
            RestreamFailed("Error.StartOutput");
            return false;
        }

        if (!PrepareOutputService())
        {
            RestreamFailed("Error.CreateRtmpService");
            return false;
        }

        if (!PrepareOutputEncoders())
        {
            RestreamFailed("Error.CreateEncoder");
            return false;
        }

        if (!PrepareEncoderSource())
        {
            RestreamFailed("Error.SceneNotExist");
            return false;
        }

        restreamState_ = RestreamState::Connecting;
        isBusy_ = true;
        if (!obs_output_start(output_)) {
            isBusy_ = false;
            RestreamFailed("Error.StartOutput");
            return false;
        }
        // obs_output_start() is asynchronous: keep the starting guard set
        // until OnStarted()/OnStopped() clears it.
        clearStarting.Dismiss();
        return true;
    }

    bool CreateObsOutput()
    {
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
            obs_data_release(output_settings);
            if (!output_) {
                return false;
            }
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

        return output_ != nullptr;
    }

    // Reads the output's last error BEFORE the output is destroyed.
    // obs_output_get_last_error() also falls back to encoder errors, so a
    // failed obs_output_start() reports the encoder's cause here.
    std::string TakeOutputLastError()
    {
        if (!output_)
            return {};
        const char* err = obs_output_get_last_error(output_);
        return err ? err : "";
    }

    void RestreamFailed(const char *message)
    {
        const std::string detail = TakeOutputLastError();
        ReleaseObsOutput();
        blog(LOG_WARNING, TAG "Local output failed for '%s' (%s)%s%s", config_->name.c_str(), message,
             detail.empty() ? "" : ": ", detail.empty() ? "" : detail.c_str());
        QString msg = QString::fromUtf8(obs_module_text(message));
        if (!detail.empty())
            msg += "\n" + QString::fromUtf8(detail.c_str());
        SetMsg(msg);
    }

    void StopStreaming() override { StopRestream(); }

    void StopRestream()
    {
        isStarting_ = false;
        if (!IsBusy() || !IsRunning()) {
            ReleaseObsOutput();
            isBusy_ = false;
            return;
        }
        if (restreamState_ == RestreamState::Stopping) return;
        bool useForce = false;
        if (isUseDelay_) {
            const auto res = QMessageBox(QMessageBox::Icon::Information, "?",
                obs_module_text("Ques.DropDelay"),
                QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No, this).exec();
            useForce = res == QMessageBox::Yes;
        }
        // A delayed output can finish while the confirmation dialog is open.
        if (!IsRunning()) { StopRestream(); return; }
        restreamState_ = RestreamState::Stopping;
        if (useForce) obs_output_force_stop(output_);
        else obs_output_stop(output_);
    }

    bool UsesSharedStreamingEncoder() const
    {
        return ResolveEncoderSourceMode(config_->videoConfig) == EncoderSourceMode::ShareStreaming ||
               ResolveEncoderSourceMode(config_->audioConfig) == EncoderSourceMode::ShareStreaming;
    }

    bool UsesSharedRecordingEncoder() const
    {
        return ResolveEncoderSourceMode(config_->videoConfig) == EncoderSourceMode::ShareRecording ||
               ResolveEncoderSourceMode(config_->audioConfig) == EncoderSourceMode::ShareRecording;
    }

    void OnOBSEvent(obs_frontend_event ev) override
    {
        using ev_t = obs_frontend_event;
        if (ev == ev_t::OBS_FRONTEND_EVENT_EXIT
            || ev == ev_t::OBS_FRONTEND_EVENT_PROFILE_CHANGED
        ) {
            Stop();
        } else if (ev == ev_t::OBS_FRONTEND_EVENT_PROFILE_LIST_CHANGED) {
            StopRestream();
        } else if (ev == ev_t::OBS_FRONTEND_EVENT_STREAMING_STARTING ||
                   ev == ev_t::OBS_FRONTEND_EVENT_STREAMING_STARTED) {
            // Prefer STARTED (shared streaming encoders are guaranteed
            // ready); STARTING is kept as a fallback so sync-start stays
            // responsive. StartStop()/StartStreaming() are exactly-once via
            // IsRunning()/restreamState_/isStarting_ guards.
            if (!IsBusy() && config_->syncStart) {
                StartStop();
            }
        } else if (ev == ev_t::OBS_FRONTEND_EVENT_RECORDING_STARTED) {
            if (!IsBusy() && config_->syncStart) {
                StartStop();
            }
        } else if (ev == ev_t::OBS_FRONTEND_EVENT_STREAMING_STOPPING ||
                   ev == ev_t::OBS_FRONTEND_EVENT_STREAMING_STOPPED) {
            if (config_->syncStop && (IsBusy() || restreamState_ != RestreamState::Stopped)) {
                StopStreaming();
            }
            // A share-streaming target cannot outlive the OBS streaming
            // encoder it borrows: stop it gracefully instead of promising
            // continued availability.
            if ((ev == ev_t::OBS_FRONTEND_EVENT_STREAMING_STOPPED ||
                 ev == ev_t::OBS_FRONTEND_EVENT_STREAMING_STOPPING) &&
                UsesSharedStreamingEncoder() && IsRunning()) {
                blog(LOG_INFO, TAG "Stopping dependent target '%s': OBS streaming stopped.",
                     config_->name.c_str());
                StopStreaming();
            }
        } else if (ev == ev_t::OBS_FRONTEND_EVENT_RECORDING_STOPPING ||
                   ev == ev_t::OBS_FRONTEND_EVENT_RECORDING_STOPPED) {
            if (UsesSharedRecordingEncoder() && IsRunning()) {
                blog(LOG_INFO, TAG "Stopping dependent target '%s': OBS recording stopped.",
                     config_->name.c_str());
                StopStreaming();
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
        return isBusy_ || isStarting_ || restreamState_ != RestreamState::Stopped;
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
        if (!enabled) {
            isStarting_ = false;
        }
        if (!enabled && IsBusy() && output_)
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
        ForceReleaseObsOutput();
        isBusy_ = false;
        QCoreApplication::removePostedEvents(this, QEvent::MetaCall);
    }

    void SetMsg(QString msg)
    {
        msg_->setText(msg);
        msg_->setToolTip(msg);
    }

    void QueueOutputUI(std::function<void()> callback)
    {
        QPointer<QObject> receiver(this);
        const auto generation = outputGeneration_.load();
        QMetaObject::invokeMethod(this, [receiver, generation, callback = std::move(callback)]() {
            auto *self = dynamic_cast<PushWidgetImpl *>(receiver.data());
            if (self && generation == self->outputGeneration_.load()) callback();
        }, Qt::QueuedConnection);
    }

    // obs logical
    void OnStarting() override
    {
        QueueOutputUI([this]() {
            restreamState_ = RestreamState::Connecting;
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
        // QueueOutputUI == RunInUIThread plus the output-generation guard
        // that drops stale callbacks from replaced outputs.
        QueueOutputUI([this]() {
            isStarting_ = false;
            isConnecting_ = false;
            isReconnecting_ = false;
            btn_->setText(obs_module_text("Status.Stop"));
            btn_->setEnabled(true);
            SetMsg(obs_module_text("Status.Streaming"));

            restreamState_ = RestreamState::Streaming;
            ResetInfo();
            timer_->start();
            UpdateStatusDot();
        });
    }

    void OnReconnect() override
    {
        QueueOutputUI([this]() {
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
        QueueOutputUI([this]() {
            isReconnecting_ = false;
            btn_->setText(obs_module_text("Status.Stop"));
            btn_->setEnabled(true);
            SetMsg(obs_module_text("Status.Streaming"));

            restreamState_ = RestreamState::Streaming;
            ResetInfo();
            timer_->start();
            UpdateStatusDot();
        });
    }

    void OnStopping() override
    {
        QueueOutputUI([this]() {
            timer_->stop();

            btn_->setText(obs_module_text("Status.Stop"));
            btn_->setEnabled(true);
            SetMsg(obs_module_text("Status.Stopping"));
        });
    }

    void OnStopped(int code) override
    {
        QueueOutputUI([this, code]() {
            isBusy_ = false;
            isStarting_ = false;
            isConnecting_ = false;
            isReconnecting_ = false;
            lastErrorCode_ = code;
            // Capture before ReleaseObsOutput() destroys the output.
            const std::string detail = TakeOutputLastError();
            ReleaseObsOutput();
            if (code != 0 && !detail.empty())
                blog(LOG_WARNING, TAG "Output '%s' stopped with code %d: %s",
                     config_->name.c_str(), code, detail.c_str());
            ResetInfo();
            timer_->stop();

            btn_->setText(obs_module_text("Btn.Start"));
            btn_->setEnabled(true);
            SetMsg(u8"");

            switch(code)
            {
                case 0:
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
            // Surface the concrete libobs/encoder cause when one was
            // recorded; the message above stays as the category.
            if (code != 0 && !detail.empty())
                SetMsg(msg_->text() + "\n" + QString::fromUtf8(detail.c_str()));

            UpdateStatusDot();
        });
    }
};

PushWidget* createPushWidget(const std::string& targetid, QWidget* parent) {
    return new PushWidgetImpl(targetid, parent);
}
