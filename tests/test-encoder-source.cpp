// Pure-logic coverage for the encoder source model (no OBS GUI needed).
// Covers: mode resolution/migration, codec-list checks, audio track
// mapping, Simple/Advanced Copy resolution, snapshot file handling.

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "encoder-source.h"
#include "helpers.h"

static int failures = 0;
#define CHECK(cond)                                                 \
    do {                                                            \
        if (!(cond)) {                                              \
            ++failures;                                             \
            std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; \
        }                                                           \
    } while (0)

int main()
{
    // Mode resolution incl. legacy migration rule.
    CHECK(ResolveEncoderSourceMode(std::string_view("<OBS_STREAMING_ENCODER>")) ==
          EncoderSourceMode::ShareStreaming);
    CHECK(ResolveEncoderSourceMode(std::string_view("<OBS_RECORDING_ENCODER>")) ==
          EncoderSourceMode::ShareRecording);
    CHECK(ResolveEncoderSourceMode(std::string_view("<OBS_STREAMING_COPY>")) ==
          EncoderSourceMode::CopyStreaming);
    CHECK(ResolveEncoderSourceMode(std::string_view("<OBS_RECORDING_COPY>")) ==
          EncoderSourceMode::CopyRecording);
    CHECK(ResolveEncoderSourceMode(std::string_view("12345")) == EncoderSourceMode::Custom);
    CHECK(ResolveEncoderSourceMode(std::optional<std::string>{}) ==
          EncoderSourceMode::ShareStreaming);
    CHECK(ResolveEncoderSourceMode(std::optional<std::string>(std::string(""))) ==
          EncoderSourceMode::ShareStreaming);
    CHECK(MigrateLegacyEncoderId("<OBS_STREAMING_ENCODER>") ==
          EncoderSourceMode::ShareStreaming);
    CHECK(MigrateLegacyEncoderId("<OBS_RECORDING_ENCODER>") ==
          EncoderSourceMode::ShareRecording);
    CHECK(MigrateLegacyEncoderId("custom-id") == EncoderSourceMode::Custom);
    CHECK(IsShareEncoderSource(EncoderSourceMode::ShareStreaming));
    CHECK(IsShareEncoderSource(EncoderSourceMode::ShareRecording));
    CHECK(!IsShareEncoderSource(EncoderSourceMode::CopyStreaming));
    CHECK(IsCopyEncoderSource(EncoderSourceMode::CopyRecording));
    CHECK(!IsCopyEncoderSource(EncoderSourceMode::Custom));
    CHECK(IsSharedObsEncoder("<OBS_STREAMING_ENCODER>"));
    CHECK(!IsSharedObsEncoder("<OBS_STREAMING_COPY>"));
    CHECK(IsCopyObsEncoder("<OBS_RECORDING_COPY>"));
    CHECK(IsSpecialEncoder("<OBS_STREAMING_COPY>"));
    CHECK(!IsSpecialEncoder("custom"));

    // Codec list semantics.
    CHECK(CodecSupportedByOutput("", "h264"));
    CHECK(CodecSupportedByOutput("h264;aac", "h264"));
    CHECK(CodecSupportedByOutput("h264;aac", "aac"));
    CHECK(!CodecSupportedByOutput("h264;aac", "hevc"));
    CHECK(CodecSupportedByOutput("", ""));
    CHECK(CodecSupportedByOutput("h264", ""));

    // Audio track mapping (display 1-6 <-> libobs 0-5).
    CHECK(IsValidDisplayAudioTrack(1) && IsValidDisplayAudioTrack(6));
    CHECK(!IsValidDisplayAudioTrack(0) && !IsValidDisplayAudioTrack(7));
    CHECK(IsValidLibobsAudioTrack(0) && IsValidLibobsAudioTrack(5));
    CHECK(!IsValidLibobsAudioTrack(6) && !IsValidLibobsAudioTrack(-1));
    CHECK(DisplayTrackToLibobs(1) == 0 && DisplayTrackToLibobs(6) == 5);
    CHECK(LibobsTrackToDisplay(0) == 1 && LibobsTrackToDisplay(5) == 6);

    // Simple-mode mapping: only unambiguous software ids resolve.
    CHECK(MapSimpleVideoEncoder("x264").value_or("") == "obs_x264");
    CHECK(!MapSimpleVideoEncoder("nvenc").has_value());
    CHECK(MapSimpleAudioEncoder("aac").value_or("") == "ffmpeg_aac");
    CHECK(!MapSimpleAudioEncoder("opus").has_value());

    // Advanced Copy resolution incl. recording-shares-streaming.
    {
        ObsProfileView adv;
        adv.outputMode = "Advanced";
        adv.advStreamVideo = "obs_x264";
        adv.advStreamAudio = "ffmpeg_aac";
        adv.advRecVideo = "none";
        adv.advRecAudio = "none";
        adv.recUsesStreamEncoder = true;
        CHECK(ResolveCopyVideoEncoderId(adv, EncoderSourceMode::CopyStreaming).value_or("") ==
              "obs_x264");
        CHECK(ResolveCopyVideoEncoderId(adv, EncoderSourceMode::CopyRecording).value_or("") ==
              "obs_x264");
        CHECK(ResolveCopyAudioEncoderId(adv, EncoderSourceMode::CopyRecording).value_or("") ==
              "ffmpeg_aac");
    }
    // Distinct recording encoder is honoured; FFmpeg recording is unresolvable.
    {
        ObsProfileView adv;
        adv.outputMode = "Advanced";
        adv.advStreamVideo = "obs_x264";
        adv.advRecVideo = "obs_qsv11";
        adv.recUsesStreamEncoder = false;
        CHECK(ResolveCopyVideoEncoderId(adv, EncoderSourceMode::CopyRecording).value_or("") ==
              "obs_qsv11");
        adv.advRecVideo = "ffmpeg_nvenc";
        CHECK(!ResolveCopyVideoEncoderId(adv, EncoderSourceMode::CopyRecording).has_value());
    }
    // Simple Copy resolution.
    {
        ObsProfileView simple;
        simple.outputMode = "Simple";
        simple.simpleStreamVideo = "x264";
        simple.simpleStreamAudio = "aac";
        CHECK(ResolveCopyVideoEncoderId(simple, EncoderSourceMode::CopyStreaming).value_or("") ==
              "obs_x264");
        CHECK(ResolveCopyAudioEncoderId(simple, EncoderSourceMode::CopyStreaming).value_or("") ==
              "ffmpeg_aac");
        // Recording mirrors streaming when no distinct entry exists.
        CHECK(ResolveCopyVideoEncoderId(simple, EncoderSourceMode::CopyRecording).value_or("") ==
              "obs_x264");
        simple.simpleStreamVideo = "nvenc";
        CHECK(!ResolveCopyVideoEncoderId(simple, EncoderSourceMode::CopyStreaming).has_value());
    }

    // Snapshot file handling with temp dirs.
    {
        namespace fs = std::filesystem;
        fs::path tmp = fs::temp_directory_path() / "obs-multi-rtmp-enc-test";
        fs::create_directories(tmp);
        // Missing file -> "{}" defaults.
        CHECK(ReadEncoderSnapshotJson(tmp.string(), "streamEncoder.json").value_or("?") == "{}");
        // Present file content passes through.
        {
            std::ofstream o(tmp / "streamEncoder.json", std::ios::binary);
            o << "{\"bitrate\":6000}";
        }
        CHECK(ReadEncoderSnapshotJson(tmp.string(), "streamEncoder.json").value_or("") ==
              "{\"bitrate\":6000}");
        // Empty dir path -> nullopt (cannot resolve).
        CHECK(!ReadEncoderSnapshotJson("", "streamEncoder.json").has_value());

        ObsProfileView adv;
        adv.outputMode = "Advanced";
        adv.advStreamVideo = "obs_x264";
        adv.advStreamAudio = "ffmpeg_aac";
        adv.advRecVideo = "none";
        adv.recUsesStreamEncoder = true;
        EncoderResolveError err = EncoderResolveError::StartOutputFailed;
        auto spec = SnapshotCopyEncoder(adv, tmp.string(), EncoderSourceMode::CopyStreaming,
                                       true, &err);
        CHECK(spec.has_value() && spec->encoderId == "obs_x264");
        CHECK(err == EncoderResolveError::None);
        CHECK(spec->settingsJson == "{\"bitrate\":6000}");
        // Recording reusing streaming falls back to streamEncoder.json.
        auto rec = SnapshotCopyEncoder(adv, tmp.string(), EncoderSourceMode::CopyRecording,
                                       true, &err);
        CHECK(rec.has_value() && rec->settingsJson == "{\"bitrate\":6000}");
        // Unresolvable FFmpeg recording reports CopyUnresolvable.
        adv.advRecVideo = "ffmpeg_nvenc";
        adv.recUsesStreamEncoder = false;
        CHECK(!SnapshotCopyEncoder(adv, tmp.string(), EncoderSourceMode::CopyRecording, true,
                                   &err).has_value());
        CHECK(err == EncoderResolveError::CopyUnresolvable);
        std::error_code ec;
        fs::remove_all(tmp, ec);
    }

    // Error keys are distinct locale keys.
    CHECK(std::string(EncoderResolveErrorKey(EncoderResolveError::SharedUnavailable)) ==
          "Error.SharedEncoderUnavailable");
    CHECK(std::string(EncoderResolveErrorKey(EncoderResolveError::CopyUnresolvable)) ==
          "Error.CopyEncoderUnresolvable");
    CHECK(std::string(EncoderResolveErrorKey(EncoderResolveError::MissingTrack)) ==
          "Error.MissingAudioTrack");

    if (failures == 0) {
        std::cout << "encoder-source: all checks passed\n";
        return 0;
    }
    std::cerr << "encoder-source: " << failures << " check(s) failed\n";
    return 1;
}
