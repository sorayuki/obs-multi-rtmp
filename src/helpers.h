#pragma once

#include <string_view>

const char * const OBS_STREAMING_ENC_PLACEHOLDER = "<OBS_STREAMING_ENCODER>";
const char * const OBS_RECORDING_ENC_PLACEHOLDER = "<OBS_RECORDING_ENCODER>";

inline bool IsSpecialEncoder(const std::string_view& encoderId) {
    return encoderId == OBS_STREAMING_ENC_PLACEHOLDER || encoderId == OBS_RECORDING_ENC_PLACEHOLDER;
}
