#include "config-serialization.h"

#include <random>
#include <unordered_map>
#include <unordered_set>

#include "json-util.hpp"

// This translation unit is intentionally logger-free and OBS-free: it must
// stay buildable in a plain host toolchain (unit tests) with no <obs.h>,
// no obs-frontend-api, and no Qt. File I/O and logging live in
// output-config.cpp, which delegates the actual (de)serialization here.

static nlohmann::json SaveTarget(OutputTargetConfig& config) {
    nlohmann::json json;
    json["id"] = config.id;
    json["name"] = config.name;
    json["protocol"] = config.protocol;
    json["service-param"] = config.serviceParam;
    json["output-param"] = config.outputParam;
    json["sync-start"] = config.syncStart;
    json["sync-stop"] = config.syncStop;
    json["auto-start"] = config.autoStart;
    json["auto-restart"] = config.autoRestart;
    json["max-restarts"] = config.maxRestarts;
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
    json["fps-denumerator"] = config.fpsDenumerator;
    return json;
}

static nlohmann::json SaveAudioTrackConfig(AudioTrackConfig &config) {
	nlohmann::json json;
	json["mixer_track"] = config.mixer_track;
	json["output_track"] = config.output_track;
	return json;
}

static nlohmann::json SaveAudioConfig(AudioEncoderConfig& config) {
    nlohmann::json json;
    json["id"] = config.id;
    json["encoder"] = config.encoderId;
    json["param"] = config.encoderParams;
    json["mixerId"] = config.mixerId;


    nlohmann::json audio_tracks(nlohmann::json::value_t::array);
    for(auto& track: config.audioTracks) {
        audio_tracks.push_back(SaveAudioTrackConfig(*track));
    }

    json["audioTracks"] = audio_tracks;

    return json;
}

static std::string SaveMultiOutputConfigImpl(MultiOutputConfig& config) {
    nlohmann::json json;

    std::unordered_set<std::string> videoconfig_in_use;
    std::unordered_set<std::string> audioconfig_in_use;

    nlohmann::json targets(nlohmann::json::value_t::array);
    for(auto& target: config.targets) {
        targets.push_back(SaveTarget(*target));
        if (target->videoConfig.has_value())
            videoconfig_in_use.insert(*target->videoConfig);
        if (target->audioConfig.has_value())
            audioconfig_in_use.insert(*target->audioConfig);
    }

    nlohmann::json video_configs(nlohmann::json::value_t::array);
    for(auto& video_config: config.videoConfig) {
        if (videoconfig_in_use.find(video_config->id) != videoconfig_in_use.end())
            video_configs.push_back(SaveVideoConfig(*video_config));
    }

    nlohmann::json audio_configs(nlohmann::json::value_t::array);
    for(auto& audio_config: config.audioConfig) {
        if (audioconfig_in_use.find(audio_config->id) != audioconfig_in_use.end())
            audio_configs.push_back(SaveAudioConfig(*audio_config));
    }

    json["targets"] = targets;
    json["video_configs"] = video_configs;
    json["audio_configs"] = audio_configs;
    if (config.hotkeysBlob.has_value())
        json["hotkeys"] = *config.hotkeysBlob;

    return json.dump();
}

static OutputTargetConfigPtr LoadTargetConfig(nlohmann::json& json) {
    auto id = GetJsonField<std::string>(json, "id");
    if (!id.has_value())
        return {};

    auto config = std::make_shared<OutputTargetConfig>();
    config->id = *id;
    config->name = GetJsonField<std::string>(json, "name").value_or("");
    config->protocol = GetJsonField<std::string>(json, "protocol").value_or("RTMP"); // for compatibility
    config->syncStart = GetJsonField<bool>(json, "sync-start").value_or(false);
    config->syncStop = GetJsonField<bool>(json, "sync-stop").value_or(config->syncStart);
    config->autoStart = GetJsonField<bool>(json, "auto-start").value_or(false);
    config->autoRestart = GetJsonField<bool>(json, "auto-restart").value_or(false);
    config->maxRestarts = GetJsonField<int>(json, "max-restarts").value_or(5);
    config->serviceParam = GetJsonField<nlohmann::json>(json, "service-param").value_or(nlohmann::json{});
    config->outputParam = GetJsonField<nlohmann::json>(json, "output-param").value_or(nlohmann::json{});
    config->videoConfig = GetJsonField<std::string>(json, "video-config");
    config->audioConfig = GetJsonField<std::string>(json, "audio-config");

    return config;
}

static VideoEncoderConfigPtr LoadVideoConfig(nlohmann::json& json) {
    auto id = GetJsonField<std::string>(json, "id");
    if (!id.has_value())
        return {};

    auto config = std::make_shared<VideoEncoderConfig>();
    config->id = *id;
    config->encoderId = GetJsonField<std::string>(json, "encoder").value_or("");
    config->outputScene = GetJsonField<std::string>(json, "scene");
    config->resolution = GetJsonField<std::string>(json, "resolution");
    config->fpsDenumerator = GetJsonField<int>(json, "fps-denumerator").value_or(1);
    config->encoderParams = GetJsonField<nlohmann::json>(json, "param").value_or(nlohmann::json{});

    return config;
}

static AudioTrackConfigPtr LoadAudioTrackConfig(nlohmann::json& json) {
    auto config = std::make_shared<AudioTrackConfig>();
    config->mixer_track = GetJsonField<int>(json, "mixer_track").value_or(0);
    config->output_track = GetJsonField<int>(json, "output_track").value_or(0);

    return config;
}

static AudioEncoderConfigPtr LoadAudioConfig(nlohmann::json& json) {
    auto id = GetJsonField<std::string>(json, "id");
    if (!id.has_value())
        return {};

    auto config = std::make_shared<AudioEncoderConfig>();
    config->id = *id;
    config->encoderId = GetJsonField<std::string>(json, "encoder").value_or("");
    config->mixerId = GetJsonField<int>(json, "mixerId").value_or(0);
    config->encoderParams = GetJsonField<nlohmann::json>(json, "param").value_or(nlohmann::json{});

    auto it = json.find("audioTracks");
    if (it != json.end() && it->type() == nlohmann::json::value_t::array) {
        for(auto& audio_track_json: *it) {
            if (audio_track_json.type() != nlohmann::json::value_t::object)
                continue;
            auto audio_track = LoadAudioTrackConfig(audio_track_json);
            if (audio_track)
                config->audioTracks.emplace_back(audio_track);
        }
    }

    return config;
}

static MultiOutputConfig LoadMultiOutputConfigImpl(const std::string& content) {
    // Note: nlohmann::json::parse throws on malformed input; that exception
    // is intentionally left to propagate to the caller (ConfigFromJsonString),
    // which is responsible for reporting it via errorOut.
    auto json = nlohmann::json::parse(content);
    MultiOutputConfig config;
    auto it = json.find("targets");
    if (it != json.end() && it->type() == nlohmann::json::value_t::array) {
        for(auto& target_json: *it) {
            if (target_json.type() != nlohmann::json::value_t::object)
                continue;
            auto target = LoadTargetConfig(target_json);
            if (target)
                config.targets.emplace_back(target);
        }
    }

    it = json.find("video_configs");
    if (it != json.end() && it->type() == nlohmann::json::value_t::array) {
        for(auto& video_enc_json: *it) {
            if (video_enc_json.type() != nlohmann::json::value_t::object)
                continue;
            auto video_enc = LoadVideoConfig(video_enc_json);
            if (video_enc) {
                config.videoConfig.emplace_back(video_enc);
            }
        }
    }

    it = json.find("audio_configs");
    if (it != json.end() && it->type() == nlohmann::json::value_t::array) {
        for(auto& audio_enc_json: *it) {
            if (audio_enc_json.type() != nlohmann::json::value_t::object)
                continue;
            auto audio_enc = LoadAudioConfig(audio_enc_json);
            if (audio_enc) {
                config.audioConfig.emplace_back(audio_enc);
            }
        }
    }

    // Additive field: legacy configs saved before this field existed simply
    // have no "hotkeys" key, so this stays nullopt and the caller (OBS-side
    // LoadConfig) skips the obs_hotkeys_load restore step entirely.
    config.hotkeysBlob = GetJsonField<std::string>(json, "hotkeys");

    return config;
}

std::string ConfigToJsonString(const MultiOutputConfig& config) {
    return SaveMultiOutputConfigImpl(const_cast<MultiOutputConfig&>(config));
}

MultiOutputConfig ConfigFromJsonString(std::string_view json, std::string* errorOut) {
    try {
        return LoadMultiOutputConfigImpl(std::string(json));
    } catch (const std::exception& e) {
        if (errorOut) *errorOut = e.what();
        return {};
    }
}

namespace {

std::string GenerateUniqueId(std::unordered_set<std::string>& usedIds) {
    static std::random_device rndgen;
    for (;;) {
        auto rndnum = rndgen();
        auto newid = std::to_string(rndnum);
        if (usedIds.find(newid) == usedIds.end())
            return newid;
    }
}

// Walks `items` in order. Any item whose id already appears in `usedIds`
// (either because it collides with something in `existing`, or because an
// earlier item in this same imported list already claimed that id) gets a
// freshly generated id. Every id assigned -- remapped or not -- is added to
// `usedIds` so later lists (and later items in the same list) see it too.
// Returns the old-id -> new-id map for ids that were actually rewritten, so
// callers can fix up references (e.g. a target's videoConfig/audioConfig).
//
// If two (or more) items in `items` share the same original id (a
// malformed/messy import), a string reference from outside this list (e.g. a
// target's videoConfig/audioConfig) cannot distinguish which of them it
// meant, so we resolve that ambiguity consistently as "first occurrence
// wins":
//   - If the FIRST item holding a given original id doesn't need to move
//     (no collision), it keeps that id -- it stays the unambiguous, still
//     valid owner, so no remap entry is recorded for it at all. Any later
//     duplicate still gets its own fresh id (to avoid a same-list id clash)
//     but must NOT be recorded as a remap target, or a reference to the
//     original id would be silently redirected away from the first
//     occurrence to this unrelated later duplicate.
//   - If the first occurrence itself has to move (it collides with
//     `existing` or an earlier list), that first remap is what gets
//     recorded; later duplicates also move but never overwrite that entry.
// `keptOriginalIds` tracks ids that some earlier item in *this* list already
// legitimately kept unchanged, so we know not to let a later duplicate's
// remap hijack that id's entry.
template<class ListT>
std::unordered_map<std::string, std::string> RemapIdsInPlace(ListT& items, std::unordered_set<std::string>& usedIds) {
    std::unordered_map<std::string, std::string> remap;
    std::unordered_set<std::string> keptOriginalIds;
    for (auto& item : items) {
        if (!item)
            continue;
        auto originalId = item->id;
        if (usedIds.find(originalId) != usedIds.end()) {
            auto newId = GenerateUniqueId(usedIds);
            if (keptOriginalIds.find(originalId) == keptOriginalIds.end())
                remap.emplace(originalId, newId);
            item->id = newId;
        } else {
            keptOriginalIds.insert(originalId);
        }
        usedIds.insert(item->id);
    }
    return remap;
}

} // namespace

void RemapImportedIds(MultiOutputConfig& imported, const MultiOutputConfig& existing) {
    // GenerateId (output-config.cpp) treats targets/videoConfig/audioConfig
    // as one shared id namespace, so we do the same here.
    std::unordered_set<std::string> usedIds;
    for (auto& t : existing.targets)
        if (t) usedIds.insert(t->id);
    for (auto& v : existing.videoConfig)
        if (v) usedIds.insert(v->id);
    for (auto& a : existing.audioConfig)
        if (a) usedIds.insert(a->id);

    auto videoRemap = RemapIdsInPlace(imported.videoConfig, usedIds);
    auto audioRemap = RemapIdsInPlace(imported.audioConfig, usedIds);
    RemapIdsInPlace(imported.targets, usedIds);

    // Targets don't have anything referencing their id, but they do
    // reference video/audio encoder-config ids -- fix those up so a target
    // still points at the (possibly renamed) encoder config it used before.
    for (auto& target : imported.targets) {
        if (!target)
            continue;
        if (target->videoConfig.has_value()) {
            auto it = videoRemap.find(*target->videoConfig);
            if (it != videoRemap.end())
                target->videoConfig = it->second;
        }
        if (target->audioConfig.has_value()) {
            auto it = audioRemap.find(*target->audioConfig);
            if (it != audioRemap.end())
                target->audioConfig = it->second;
        }
    }
}
