#include "output-config.h"

#include <obs.h>
#include <random>
#include "json-util.hpp"

static nlohmann::json SaveTarget(OutputTargetConfig& config) {
    nlohmann::json json;
    json["id"] = config.id;
    json["name"] = config.name;
    json["service-path"] = config.servicePath;
    json["service-key"] = config.serviceKey;
    json["service-user"] = config.serviceUser;
    json["service-pass"] = config.servicePass;
    json["sync-start"] = config.syncStart;
    if (config.videoConfig.has_value())
        json["video-config"] = *config.videoConfig;
    if (config.audioConfig.has_value())
        json["audio-config"] = *config.audioConfig;
    return json;
}

static nlohmann::json SaveVideoConfig(VideoEncoderConfig& config) {
    nlohmann::json json;
    json["id"] = config.id;
    json["encoder"] = config.encoderId;
    json["param"] = config.encoderParams;
    if (config.outputScene.has_value())
        json["scene"] = *config.outputScene;
    if (config.resolution.has_value())
        json["resolution"] = *config.resolution;
    return json;
}

static nlohmann::json SaveAudioConfig(AudioEncoderConfig& config) {
    nlohmann::json json;
    json["id"] = config.id;
    json["encoder"] = config.encoderId;
    json["param"] = config.encoderParams;
    json["mixerId"] = config.mixerId;
    return json;
}

std::string SaveMultiOutputConfig(MultiOutputConfig& config) {
    nlohmann::json json;

    nlohmann::json targets(nlohmann::json::value_t::array);
    for(auto& target: config.targets) {
        targets.push_back(SaveTarget(target.second));
    }

    nlohmann::json video_configs(nlohmann::json::value_t::array);
    for(auto& video_config: config.videoConfig) {
        video_configs.push_back(SaveVideoConfig(video_config.second));
    }

    nlohmann::json audio_configs(nlohmann::json::value_t::array);
    for(auto& audio_config: config.audioConfig) {
        audio_configs.push_back(SaveAudioConfig(audio_config.second));
    }

    json["targets"] = targets;
    json["video_configs"] = video_configs;
    json["audio_configs"] = audio_configs;

    return json.dump();
}




static std::optional<OutputTargetConfig> LoadTargetConfig(nlohmann::json& json) {
    auto name = GetJsonField<std::string>(json, "name");
    if (!name.has_value())
        return {};

    OutputTargetConfig config;
    config.name = *name;
    config.syncStart = GetJsonField<bool>(json, "sync-start").value_or(false);
    config.servicePath = GetJsonField<std::string>(json, "service-path").value_or("");
    config.serviceKey = GetJsonField<std::string>(json, "service-key").value_or("");
    config.serviceUser = GetJsonField<std::string>(json, "service-user").value_or("");
    config.servicePass = GetJsonField<std::string>(json, "service-pass").value_or("");
    config.videoConfig = GetJsonField<std::string>(json, "video-config");
    config.audioConfig = GetJsonField<std::string>(json, "audio-config");

    return config;
}

static std::optional<VideoEncoderConfig> LoadVideoConfig(nlohmann::json& json) {
    auto id = GetJsonField<std::string>(json, "id");
    if (!id.has_value())
        return {};

    VideoEncoderConfig config;
    config.id = *id;
    config.encoderId = GetJsonField<std::string>(json, "encoder").value_or("");
    config.outputScene = GetJsonField<std::string>(json, "scene");
    config.resolution = GetJsonField<std::string>(json, "resolution");
    config.encoderParams = GetJsonField<nlohmann::json>(json, "param").value_or(nlohmann::json{});

    return config;
}

static std::optional<AudioEncoderConfig> LoadAudioConfig(nlohmann::json& json) {
    auto id = GetJsonField<std::string>(json, "id");
    if (!id.has_value())
        return {};
    
    AudioEncoderConfig config;
    config.id = *id;
    config.encoderId = GetJsonField<std::string>(json, "encoder").value_or("");
    config.mixerId = GetJsonField<int>(json, "mixerId").value_or(0);
    config.encoderParams = GetJsonField<nlohmann::json>(json, "param").value_or(nlohmann::json{});

    return config;
}

MultiOutputConfig LoadMultiOutputConfig(const std::string& content) {
    try {
        auto json = nlohmann::json::parse(content);
        MultiOutputConfig config;
        auto it = json.find("targets");
        if (it != json.end() && it->type() == nlohmann::json::value_t::array) {
            for(auto& target_json: *it) {
                if (target_json.type() != nlohmann::json::value_t::object)
                    continue;
                auto target = LoadTargetConfig(target_json);
                if (target.has_value())
                    config.targets.insert(std::make_pair(target->id, *target));
            }
        }

        it = json.find("video_configs");
        if (it != json.end() && it->type() == nlohmann::json::value_t::array) {
            for(auto& video_enc_json: *it) {
                if (video_enc_json.type() != nlohmann::json::value_t::object)
                    continue;
                auto video_enc = LoadVideoConfig(video_enc_json);
                if (video_enc.has_value()) {
                    auto id = video_enc->id;
                    config.videoConfig.insert(std::make_pair(id, std::move(*video_enc)));
                }
            }
        }

        it = json.find("video_configs");
        if (it != json.end() && it->type() == nlohmann::json::value_t::array) {
            for(auto& audio_enc_json: *it) {
                if (audio_enc_json.type() != nlohmann::json::value_t::object)
                    continue;
                auto audio_enc = LoadAudioConfig(audio_enc_json);
                if (audio_enc.has_value()) {
                    auto id = audio_enc->id;
                    config.audioConfig.insert(std::make_pair(id, std::move(*audio_enc)));
                }
            }
        }
        
        return config;
    }
    catch(const std::exception& e) {
        blog(LOG_ERROR, "[LoadMultiOutputConfig] Fail to parse config json: %s", e.what());
        return {};
    }
}


std::string GenerateId(MultiOutputConfig& config) {
    static std::random_device rndgen;
    for(;;) {
        auto rndnum = rndgen();
        auto newid = std::to_string(rndnum);
        if (config.targets.find(newid) != config.targets.end())
            continue;
        if (config.audioConfig.find(newid) != config.audioConfig.end())
            continue;
        if (config.videoConfig.find(newid) != config.videoConfig.end())
            continue;
        return newid;
    }
}