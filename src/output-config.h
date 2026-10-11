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

struct AudioTrackConfig {
    int mixer_track;
    int output_track;
};
using AudioTrackConfigPtr = std::shared_ptr<AudioTrackConfig>;

struct AudioEncoderConfig {
    std::string id;
    std::string encoderId;
    nlohmann::json encoderParams;
    int mixerId = 0;
    std::list<AudioTrackConfigPtr> audioTracks;
};
using AudioEncoderConfigPtr = std::shared_ptr<AudioEncoderConfig>; 


struct OutputTargetConfig {
    std::string id;
    std::string name;
    std::string protocol = "RTMP";
    bool syncStart = false;
    bool syncStop = false;
    // When false, this target is skipped by "Start all" / sync-start and
    // StartStreaming() refuses to start it, without losing its saved
    // settings. Lets a target be temporarily excluded (e.g. a platform
    // that's down) without deleting and re-creating it.
    bool enabled = true;

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

std::string GenerateId(MultiOutputConfig& config);

// Used by the dock's Export/Import Config buttons to back up or restore
// all targets (including stream keys) outside of OBS's own profile
// folder, since the plugin otherwise only ever saves into the current
// profile's obs-multi-rtmp.json.
std::string SerializeMultiOutputConfig(MultiOutputConfig& config);

// Returns std::nullopt only if `content` isn't valid JSON; a validly
// parsed but unexpectedly-shaped file yields an (possibly empty) config
// instead of failing, same tolerance as the profile loader.
std::optional<MultiOutputConfig> DeserializeMultiOutputConfig(const std::string& content);
