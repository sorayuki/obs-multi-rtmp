#pragma once

#include <string>
#include <optional>
#include <vector>
#include <unordered_map>
#include <memory>
#include <list>

#include <json.hpp>

struct VideoEncoderConfig {
    std::string id;
    std::string encoderId;
    int fpsDenumerator = 1;
    nlohmann::json encoderParams;
    std::optional<std::string> outputScene;
    std::optional<std::string> resolution;
};
using VideoEncoderConfigPtr = std::shared_ptr<VideoEncoderConfig>;


struct AudioEncoderConfig {
    std::string id;
    std::string encoderId;
    nlohmann::json encoderParams;
    int mixerId = 0;
};
using AudioEncoderConfigPtr = std::shared_ptr<AudioEncoderConfig>; 


struct OutputTargetConfig {
    std::string id;
    std::string name;
    std::string protocol;
    bool syncStart = false;
    bool syncStop = false;

    nlohmann::json serviceParam;
    nlohmann::json outputParam;

    std::optional<std::string> videoConfig;
    std::optional<std::string> audioConfig;
};
using OutputTargetConfigPtr = std::shared_ptr<OutputTargetConfig>;


struct MultiOutputConfig {
public:
    std::list<OutputTargetConfigPtr> targets;
    std::list<VideoEncoderConfigPtr> videoConfig;
    std::list<AudioEncoderConfigPtr> audioConfig;
};

template<class T, class S>
inline T FindById(std::list<T>& list, const S& id) {
    for(auto& x: list) {
        if (x->id == id)
            return x;
    }
    return nullptr;
}


MultiOutputConfig& GlobalMultiOutputConfig();

void SaveMultiOutputConfig();

bool LoadMultiOutputConfig();

void ImportLegacyMultiOutputConfig();

std::string GenerateId(MultiOutputConfig& config);
