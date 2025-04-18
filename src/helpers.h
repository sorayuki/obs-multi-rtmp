#pragma once

#include <string_view>
#include <optional>
#include <QString>

const char * const OBS_STREAMING_ENC_PLACEHOLDER = "<OBS_STREAMING_ENCODER>";
const char * const OBS_RECORDING_ENC_PLACEHOLDER = "<OBS_RECORDING_ENCODER>";

// Do not change this order without considering backwards compatibility.
// It's used for sorting and assigning indices to encoders in the UI and
// may break loaded configs if changed.
enum SpecialEncoderType
{
    OBS_STREAMING_ENC = 0,
    OBS_RECORDING_ENC = 1,
};

bool IsSpecialEncoder(const QString& id);
std::optional<SpecialEncoderType> GetSpecialEncoderType(const std::string_view id);
