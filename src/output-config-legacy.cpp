#include "pch.h"

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


static VideoEncoderConfigPtr ImportLegacyVideoConfig(QJsonObject& json) {
    auto it = json.find("v-enc");
    if (it == json.end()) {
        return {};
    }

    auto config = std::make_shared<VideoEncoderConfig>();
    config->encoderId = tostdu8(it->toString());
    
    it = json.find("v-scene");
    if (it != json.end() && it->isString())
        config->outputScene = tostdu8(it->toString());

    it = json.find("v-resolution");
    if (it != json.end() && it->isString())
        config->resolution = tostdu8(it->toString());

    it = json.find("v-bitrate");
    if (it != json.end() && it->isDouble())
        config->encoderParams["bitrate"] = it->toInt();

    it = json.find("v-keyframe-sec");
    if (it != json.end() && it->isDouble())
        config->encoderParams["keyint_sec"] = it->toInt();

    it = json.find("v-bframes");
    if (it != json.end() && it->isDouble())
        config->encoderParams["bf"] = it->toInt();

    return config;
}


static AudioEncoderConfigPtr ImportLegacyAudioConfig(QJsonObject& json) {
    auto it = json.find("a-enc");
    if (it == json.end()) {
        return {};
    }

    auto config = std::make_shared<AudioEncoderConfig>();
    config->encoderId = tostdu8(it->toString());

    it = json.find("a-mixer");
    if (it != json.end() && it->isDouble())
        config->mixerId = it->toInt();
    
    it = json.find("a-bitrate");
    if (it != json.end() && it->isDouble())
        config->encoderParams["bitrate"] = it->toInt();
    
    return config;
}


static OutputTargetConfigPtr ImportLegacyTargetConfig(QJsonObject json, MultiOutputConfig& parentConfig) {
    auto config = std::make_shared<OutputTargetConfig>();

    auto it = json.find("name");
    if (it != json.end() && it->isString())
        config->name = tostdu8(it->toString());

    it = json.find("syncstart");
    if (it != json.end() && it->isBool())
        config->syncStart = it->toBool();

    it = json.find("rtmp-path");
    if (it != json.end() && it->isString())
        config->serviceParam["server"] = tostdu8(it->toString());
    
    it = json.find("rtmp-key");
    if (it != json.end() && it->isString())
        config->serviceParam["key"] = tostdu8(it->toString());
    
    it = json.find("rtmp-user");
    if (it != json.end() && it->isString()) {
        auto username = tostdu8(it->toString());
        if (username.empty()) {
            config->serviceParam["use_auth"] = false;
        } else {
            config->serviceParam["use_auth"] = true;
            config->serviceParam["username"] = username;
            it = json.find("rtmp-pass");
            if (it != json.end() && it->isString())
                config->serviceParam["password"] = tostdu8(it->toString());
        }
    }
    
    auto audioConfig = ImportLegacyAudioConfig(json);
    if (audioConfig) {
        audioConfig->encoderId = GenerateId(parentConfig);
        parentConfig.audioConfig.emplace_back(audioConfig);
    }

    auto videoConfig = ImportLegacyVideoConfig(json);
    if (videoConfig) {
        videoConfig->encoderId = GenerateId(parentConfig);
        parentConfig.videoConfig.emplace_back(videoConfig);
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
        new_target->id = GenerateId(config);
        config.targets.emplace_back(new_target);
    }

    return config;
}


void ImportLegacyMultiOutputConfig() {
    blog(LOG_INFO, TAG "Import config from old version.");
    auto profile_config = obs_frontend_get_profile_config();

    QJsonObject conf;
    auto base64str = config_get_string(profile_config, ConfigSection, "json");
    if (!base64str || !*base64str) { // compatible with old version
        base64str = config_get_string(obs_frontend_get_app_config(), ConfigSection, "json");
    }

    if (base64str && *base64str)
    {
        auto bindat = QByteArray::fromBase64(base64str);
        auto jsondoc = QJsonDocument::fromJson(bindat);
        if (jsondoc.isObject()) {
            conf = jsondoc.object();

            // import legacy config
            GlobalMultiOutputConfig() = ImportLegacyMultiOutputConfig(conf);
            SaveMultiOutputConfig();

            // erase legacy config
            // config_set_string(obs_frontend_get_app_config(), ConfigSection, "json", "");
        }
    }
}
