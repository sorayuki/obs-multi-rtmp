#pragma once

#include <string>
#include <optional>
#include <vector>
#include <unordered_map>

#include <json.hpp>

struct VideoEncoderConfig {
    std::string id;
    std::string encoderId;
    nlohmann::json encoderParams;
    std::optional<std::string> outputScene;
    std::optional<std::string> resolution;
};


struct AudioEncoderConfig {
    std::string id;
    std::string encoderId;
    nlohmann::json encoderParams;
    int mixerId = 0;
};


struct OutputTargetConfig {
    std::string id;
    std::string name;
    std::string servicePath;
    std::string serviceKey;
    std::string serviceUser;
    std::string servicePass;

    bool syncStart = false;

    std::optional<std::string> videoConfig;
    std::optional<std::string> audioConfig;
};


struct MultiOutputConfig {
    std::unordered_map<std::string, OutputTargetConfig> targets;
    std::unordered_map<std::string, VideoEncoderConfig> videoConfig;
    std::unordered_map<std::string, AudioEncoderConfig> audioConfig;
};


std::string SaveMultiOutputConfig(MultiOutputConfig& config);

MultiOutputConfig LoadMultiOutputConfig(const std::string& content);

MultiOutputConfig ImportLegacyMultiOutputConfig();

std::string GenerateId(MultiOutputConfig& config);
