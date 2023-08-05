#include "output-config.h"

#include <obs.h>

std::string SaveMultiOutputConfig(MultiOutputConfig& config) {
    nlohmann::json json;
    return json.dump();
}


static OutputTargetConfig LoadTargetConfig() {
    OutputTargetConfig config;
    return config;
}

static VideoEncoderConfig LoadVideoConfig() {
    VideoEncoderConfig config;
    return config;
}

static AudioEncoderConfig LoadAudioConfig() {
    AudioEncoderConfig config;
    return config;
}

MultiOutputConfig LoadMultiOutputConfig(const std::string& content) {
    try {
        auto json = nlohmann::json::parse(content);
        MultiOutputConfig config;
        
        return config;
    }
    catch(const std::exception& e) {
        blog(LOG_ERROR, "[LoadMultiOutputConfig] Fail to parse config json.");
        return {};
    }
}

