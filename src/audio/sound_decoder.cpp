#include "ts3toolkit/audio/sound_decoder.h"

#include <miniaudio.h>
#include <filesystem>

namespace ts3toolkit::audio {

namespace {

DecodeStatus classifyInitError(ma_result result) {
    switch (result) {
        case MA_DOES_NOT_EXIST:
        case MA_IS_DIRECTORY:
        case MA_PATH_TOO_LONG:
            return DecodeStatus::kFileNotFound;
        case MA_FORMAT_NOT_SUPPORTED:
        case MA_INVALID_FILE:
            return DecodeStatus::kUnsupportedFormat;
        default:
            return DecodeStatus::kDecodeFailed;
    }
}

}  // namespace

DecodeResult SoundDecoder::decodeFile(const std::string& path) const {
    DecodeResult result;

    ma_decoder_config config = ma_decoder_config_init(
        ma_format_s16, core::kChannelCount, core::kSampleRateHz);

    ma_decoder decoder;
#if defined(_WIN32)
    ma_result init_result;
    try {
        init_result = ma_decoder_init_file_w(
            std::filesystem::u8path(path).c_str(), &config, &decoder);
    } catch (const std::exception&) {
        result.status = DecodeStatus::kFileNotFound;
        return result;
    }
#else
    ma_result init_result = ma_decoder_init_file(path.c_str(), &config, &decoder);
#endif
    if (init_result != MA_SUCCESS) {
        result.status = classifyInitError(init_result);
        return result;
    }

    ma_uint64 total_frames = 0;
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &total_frames) !=
        MA_SUCCESS) {
        result.status = DecodeStatus::kDecodeFailed;
        ma_decoder_uninit(&decoder);
        return result;
    }

    if (total_frames > kMaxEntranceSoundSamples) {
        result.status = DecodeStatus::kTooLong;
        ma_decoder_uninit(&decoder);
        return result;
    }

    result.sound.samples.resize(static_cast<std::size_t>(total_frames));
    ma_uint64 frames_read = 0;
    ma_result read_result =
        ma_decoder_read_pcm_frames(&decoder, result.sound.samples.data(),
                                   total_frames, &frames_read);
    ma_decoder_uninit(&decoder);

    if (read_result != MA_SUCCESS || frames_read != total_frames) {
        result.sound.samples.clear();
        result.status = DecodeStatus::kDecodeFailed;
        return result;
    }

    result.status = DecodeStatus::kOk;
    return result;
}

}  // namespace ts3toolkit::audio
