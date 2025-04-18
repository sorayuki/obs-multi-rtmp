#include "helpers.h"

bool IsSpecialEncoder(const QString& id)
{
    return id == OBS_STREAMING_ENC_PLACEHOLDER || id == OBS_RECORDING_ENC_PLACEHOLDER;
}

std::optional<SpecialEncoderType> GetSpecialEncoderType(const std::string_view id)
{
    if (id == OBS_STREAMING_ENC_PLACEHOLDER)
        return OBS_STREAMING_ENC;
    else if (id == OBS_RECORDING_ENC_PLACEHOLDER)
        return OBS_RECORDING_ENC;
    else
        return std::nullopt;
}