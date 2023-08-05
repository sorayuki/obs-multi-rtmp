#include "output-config.h"

#include "obs.h"
#include "obs-frontend-api.h"
#include "util/config-file.h"

#include <regex>
#include <random>

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#define ConfigSection "obs-multi-rtmp"


static std::optional<VideoEncoderConfig> ImportLegacyVideoConfig(QJsonObject& json) {
    auto it = json.find("v-enc");
    if (it == json.end()) {
        return {};
    }

    VideoEncoderConfig config;
    config.encoderId = it->toString().toUtf8().constData();
    
    it = json.find("v-scene");
    if (it != json.end() && it->isString())
        config.outputScene = it->toString().toUtf8().constData();

    it = json.find("v-resolution");
    if (it != json.end() && it->isString())
        config.resolution = it->toString().toUtf8().constData();

    it = json.find("v-bitrate");
    if (it != json.end() && it->isDouble())
        config.encoderParams["bitrate"] = it->toInt();

    it = json.find("v-keyframe-sec");
    if (it != json.end() && it->isDouble())
        config.encoderParams["keyint_sec"] = it->toInt();

    it = json.find("v-bframes");
    if (it != json.end() && it->isDouble())
        config.encoderParams["bf"] = it->toInt();

    return config;
}


static std::optional<AudioEncoderConfig> ImportLegacyAudioConfig(QJsonObject& json) {
    auto it = json.find("a-enc");
    if (it == json.end()) {
        return {};
    }

    AudioEncoderConfig config;
    config.encoderId = it->toString().toUtf8().constData();

    it = json.find("a-mixer");
    if (it != json.end() && it->isDouble())
        config.mixerId = it->toInt();
    
    it = json.find("a-bitrate");
    if (it != json.end() && it->isDouble())
        config.encoderParams["bitrate"] = it->toInt();
    
    return config;
}


static OutputTargetConfig ImportLegacyTargetConfig(QJsonObject json, MultiOutputConfig& parentConfig) {
    OutputTargetConfig config;

    auto it = json.find("name");
    if (it != json.end() && it->isString())
        config.name = it->toString().toUtf8().constData();

    it = json.find("syncstart");
    if (it != json.end() && it->isBool())
        config.syncStart = it->toBool();

    it = json.find("rtmp-path");
    if (it != json.end() && it->isString())
        config.servicePath = it->toString().toUtf8().constData();
    
    it = json.find("rtmp-key");
    if (it != json.end() && it->isString())
        config.serviceKey = it->toString().toUtf8().constData();
    
    it = json.find("rtmp-user");
    if (it != json.end() && it->isString())
        config.serviceUser = it->toString().toUtf8().constData();
    
    it = json.find("rtmp-pass");
    if (it != json.end() && it->isString())
        config.servicePass = it->toString().toUtf8().constData();

    auto audioConfig = ImportLegacyAudioConfig(json);
    if (audioConfig.has_value()) {
        audioConfig->encoderId = GenerateId(parentConfig);
        parentConfig.audioConfig.insert(
            std::make_pair(audioConfig->encoderId, *audioConfig)
        );
    }

    auto videoConfig = ImportLegacyVideoConfig(json);
    if (videoConfig.has_value()) {
        videoConfig->encoderId = GenerateId(parentConfig);
        parentConfig.videoConfig.insert(
            std::make_pair(videoConfig->encoderId, *videoConfig)
        );
    }

    return config;
}


static MultiOutputConfig ImportLegacyMultiOutputConfig(QJsonObject& json) {
    MultiOutputConfig config;
    
    auto targets = json.find("targets");
    if (targets == json.end() || !targets->isArray())
        return {};
    for(auto x : targets->toArray())
    {
        if (!x.isObject())
            continue;
        auto new_target = ImportLegacyTargetConfig(x.toObject(), config);
        new_target.id = GenerateId(config);
        config.targets.insert(std::make_pair(new_target.id, new_target));
    }

    return config;
}


MultiOutputConfig ImportLegacyMultiOutputConfig() {
    auto profile_config = obs_frontend_get_profile_config();

    QJsonObject conf;
    auto base64str = config_get_string(profile_config, ConfigSection, "json");
    if (!base64str || !*base64str) { // compatible with old version
        base64str = config_get_string(obs_frontend_get_global_config(), ConfigSection, "json");
    }

    if (base64str && *base64str)
    {
        auto bindat = QByteArray::fromBase64(base64str);
        auto jsondoc = QJsonDocument::fromJson(bindat);
        if (jsondoc.isObject()) {
            conf = jsondoc.object();

            // import legacy config
            return ImportLegacyMultiOutputConfig(conf);
        }
    }

    return {};
}
