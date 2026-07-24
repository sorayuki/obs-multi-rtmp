#include "output-config.h"
#include "pch.h"

#include <obs.h>
#include <obs-frontend-api.h>
#include <random>
#include <filesystem>
#include <algorithm>
#include <util/platform.h>
#include "json-util.hpp"
#include "config-serialization.h"


MultiOutputConfig& GlobalMultiOutputConfig()
{
    static MultiOutputConfig instance;
    return instance;
}


void SaveMultiOutputConfig() {
    auto profiledir = obs_frontend_get_current_profile_path();
    if (profiledir) {
        std::string filename = std::string(profiledir) + "/obs-multi-rtmp.json";
        auto content = ConfigToJsonString(GlobalMultiOutputConfig());
        os_quick_write_utf8_file_safe(filename.c_str(), content.c_str(), content.size(), true, "tmp", "bak");
        blog(LOG_INFO, TAG "Save config into %s", filename.c_str());
    }
    bfree(profiledir);
}

bool LoadMultiOutputConfig() {
    auto profiledir = obs_frontend_get_current_profile_path();
    bool ret = false;
    if (profiledir) {
        std::string filename = std::string(profiledir) + "/obs-multi-rtmp.json";
        auto content = os_quick_read_utf8_file(filename.c_str());
        if (content) {
            std::string err;
            GlobalMultiOutputConfig() = ConfigFromJsonString(content, &err);
            if (!err.empty()) blog(LOG_ERROR, TAG "Config parse error: %s", err.c_str());
            bfree(content);
            ret = true;
        }
    }
    bfree(profiledir);
    return ret;
}


template<class T>
static bool has_id(T& container, const std::string& id) {
    for(auto& item: container) {
        if (item->id == id)
            return true;
    }
    return false;
}

std::string GenerateId(MultiOutputConfig& config) {
    static std::random_device rndgen;
    for(;;) {
        auto rndnum = rndgen();
        auto newid = std::to_string(rndnum);
        if (has_id(config.targets, newid))
            continue;
        if (has_id(config.audioConfig, newid))
            continue;
        if (has_id(config.videoConfig, newid))
            continue;
        return newid;
    }
}
