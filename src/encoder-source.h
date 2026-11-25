#pragma once

// Central encoder resolver/preflight helpers that run without an OBS GUI.
// Pure functions here are covered by tests/test-encoder-source.cpp. OBS
// object access (frontend outputs, live encoders) stays in push-widget.cpp;
// profile-dir file I/O uses only std::ifstream so tests can use temp dirs.

#include <optional>
#include <string>

#include "helpers.h"

// Typed preflight outcome. Each value maps to a localized UI string via
// EncoderResolveErrorKey() (locale keys live in data/locale/*.ini).
enum class EncoderResolveError {
    None = 0,
    SharedUnavailable,
    CopyUnresolvable,
    UnsupportedVideoCodec,
    UnsupportedAudioCodec,
    MissingVideoConfig,
    MissingAudioConfig,
    InvalidVideoConfig,
    InvalidAudioConfig,
    MissingTrack,
    StartOutputFailed,
};

const char *EncoderResolveErrorKey(EncoderResolveError err);

// Lightweight view of the OBS profile needed to resolve Copy modes.
// Filled in push-widget.cpp from config_t (profile basic.ini); in tests it
// is constructed directly. Empty strings mean "not configured".
struct ObsProfileView {
    // "Simple" or "Advanced" (value of Output/Mode).
    std::string outputMode;
    // Simple mode short names, e.g. "x264", "aac".
    std::string simpleStreamVideo;
    std::string simpleStreamAudio;
    std::string simpleRecVideo;
    std::string simpleRecAudio;
    // Advanced mode full encoder ids, e.g. "obs_x264".
    std::string advStreamVideo;
    std::string advStreamAudio;
    std::string advRecVideo;
    std::string advRecAudio;
    // Recording reuse flag: RecEncoder == "none" means the recording uses
    // the streaming encoder (no distinct recording encoder exists).
    bool recUsesStreamEncoder = false;
};

// Resolve which configured encoder id a Copy mode should snapshot.
// Returns nullopt when the configuration cannot be reconstructed reliably
// (e.g. FFmpeg/custom recording outputs); the caller must then fail with
// EncoderResolveError::CopyUnresolvable, never silently substitute.
std::optional<std::string> ResolveCopyVideoEncoderId(const ObsProfileView &profile,
                                                     EncoderSourceMode mode);
std::optional<std::string> ResolveCopyAudioEncoderId(const ObsProfileView &profile,
                                                     EncoderSourceMode mode);

// Map a Simple-mode short encoder name to a full libobs encoder id where
// the mapping is unambiguous across platforms. Returns nullopt when the
// name needs platform-specific disambiguation the caller cannot prove.
std::optional<std::string> MapSimpleVideoEncoder(const std::string &shortName);
std::optional<std::string> MapSimpleAudioEncoder(const std::string &shortName);

// Read a snapshot settings JSON file (streamEncoder.json /
// recordEncoder.json) from a profile directory. Returns the raw JSON text
// ("{}" when the file is missing but the encoder id is known, so defaults
// apply); nullopt when the directory/file cannot be read AND no caller
// default should be assumed... In practice: missing file -> "{}".
// Unreadable directory -> nullopt.
std::optional<std::string> ReadEncoderSnapshotJson(const std::string &profileDir,
                                                  const char *fileName);

// Full Copy snapshot: resolved encoder id + settings JSON text.
// nullopt + error out-param when unresolvable.
struct CopiedEncoderSpec {
    std::string encoderId;
    std::string settingsJson;
};

std::optional<CopiedEncoderSpec> SnapshotCopyEncoder(const ObsProfileView &profile,
                                                    const std::string &profileDir,
                                                    EncoderSourceMode mode,
                                                    bool isVideo,
                                                    EncoderResolveError *error = nullptr);
