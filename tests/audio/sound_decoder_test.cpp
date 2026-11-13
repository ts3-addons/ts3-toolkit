#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "tests/test_macros.h"
#include "ts3toolkit/audio/sound_decoder.h"

using namespace ts3toolkit::audio;
namespace fs = std::filesystem;

namespace {

void putU32(std::ofstream& out, std::uint32_t value) {
    char bytes[4] = {static_cast<char>(value & 0xff),
                     static_cast<char>((value >> 8) & 0xff),
                     static_cast<char>((value >> 16) & 0xff),
                     static_cast<char>((value >> 24) & 0xff)};
    out.write(bytes, 4);
}

void putU16(std::ofstream& out, std::uint16_t value) {
    char bytes[2] = {static_cast<char>(value & 0xff),
                     static_cast<char>((value >> 8) & 0xff)};
    out.write(bytes, 2);
}

// Write a 48 kHz mono s16 PCM WAV. declared_frames lets the header claim a
// length larger than the actual payload to test the duration guard.
void writeWav(const std::string& path,
              const std::vector<std::int16_t>& samples,
              std::uint32_t declared_frames = 0) {
    const std::uint32_t frames =
        declared_frames != 0 ? declared_frames
                             : static_cast<std::uint32_t>(samples.size());
    const std::uint32_t data_size = frames * 2;

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write("RIFF", 4);
    putU32(out, 36 + data_size);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    putU32(out, 16);
    putU16(out, 1);                 // PCM
    putU16(out, 1);                 // mono
    putU32(out, 48000);             // sample rate
    putU32(out, 48000 * 2);         // byte rate
    putU16(out, 2);                 // block align
    putU16(out, 16);                // bits per sample
    out.write("data", 4);
    putU32(out, data_size);
    out.write(reinterpret_cast<const char*>(samples.data()),
              static_cast<std::streamsize>(samples.size() * 2));
}

std::string tempPath(const std::string& name) {
    return (fs::temp_directory_path() / ("ts3toolkit_dec_" + name)).string();
}

// Write a WAV whose header and actual payload both contain frame_count frames.
// The payload is filled from a reusable zero buffer so a multi-second file can
// be written without per-sample overhead. Used to exercise the duration guard,
// because miniaudio reports a truncated file by its real payload length.
void writeWavFilled(const std::string& path, std::uint32_t frame_count) {
    const std::uint32_t data_size = frame_count * 2;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write("RIFF", 4);
    putU32(out, 36 + data_size);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    putU32(out, 16);
    putU16(out, 1);
    putU16(out, 1);
    putU32(out, 48000);
    putU32(out, 48000 * 2);
    putU16(out, 2);
    putU16(out, 16);
    out.write("data", 4);
    putU32(out, data_size);

    constexpr std::uint32_t kChunkSamples = 65536;
    const std::vector<std::int16_t> zeros(kChunkSamples, 0);
    std::uint32_t remaining = frame_count;
    while (remaining > 0) {
        const std::uint32_t n =
            remaining < kChunkSamples ? remaining : kChunkSamples;
        out.write(reinterpret_cast<const char*>(zeros.data()),
                  static_cast<std::streamsize>(n) * 2);
        remaining -= n;
    }
}

}  // namespace

void runTests() {
    const std::string wav_path = tempPath("ok.wav");
    const std::vector<std::int16_t> samples{0, 100, -100, 32767, -32768, 1234};
    writeWav(wav_path, samples);

    SoundDecoder decoder;
    DecodeResult ok = decoder.decodeFile(wav_path);
    CHECK_EQ(ok.status, DecodeStatus::kOk);
    CHECK_EQ(ok.sound.lengthSamples(), samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i) {
        CHECK_EQ(ok.sound.samples[i], samples[i]);
    }

    const fs::path unicode_path = fs::temp_directory_path() /
        fs::u8path(u8"ts3toolkit_音效.wav");
    fs::copy_file(wav_path, unicode_path, fs::copy_options::overwrite_existing);
    CHECK_EQ(decoder.decodeFile(unicode_path.u8string()).status, DecodeStatus::kOk);
    fs::remove(unicode_path);
#if defined(_WIN32)
    CHECK_EQ(decoder.decodeFile(std::string("\xff", 1)).status,
             DecodeStatus::kFileNotFound);
#endif

    // Missing file.
    DecodeResult missing = decoder.decodeFile(tempPath("missing.wav"));
    CHECK_EQ(missing.status, DecodeStatus::kFileNotFound);

    // Garbage content with a wav extension.
    const std::string bad_path = tempPath("bad.wav");
    std::ofstream bad(bad_path, std::ios::binary | std::ios::trunc);
    bad << "not a real wav file content at all";
    bad.close();
    DecodeResult invalid = decoder.decodeFile(bad_path);
    CHECK_TRUE(invalid.status == DecodeStatus::kUnsupportedFormat ||
               invalid.status == DecodeStatus::kDecodeFailed);
    CHECK_TRUE(invalid.sound.empty());

    // A real file longer than the 30 second limit is rejected.
    const std::string long_path = tempPath("long.wav");
    writeWavFilled(long_path, 48000 * 31);
    DecodeResult too_long = decoder.decodeFile(long_path);
    CHECK_EQ(too_long.status, DecodeStatus::kTooLong);
    DecodeResult source = decoder.decodeFile(long_path, kMaxSourceSoundSamples);
    CHECK_EQ(source.status, DecodeStatus::kOk);
    CHECK_EQ(selectRange(source.sound, 0, -1).status, DecodeStatus::kInvalidRange);
    DecodeResult segment = selectRange(source.sound, 1000, 31000);
    CHECK_EQ(segment.status, DecodeStatus::kOk);
    CHECK_EQ(segment.sound.lengthSamples(), kMaxEntranceSoundSamples);
    CHECK_EQ(selectRange(source.sound, 1000, 1000).status, DecodeStatus::kInvalidRange);
    CHECK_EQ(selectRange(source.sound, 0, 31001).status, DecodeStatus::kInvalidRange);
    CHECK_EQ(selectRange(source.sound, -1, 100).status, DecodeStatus::kInvalidRange);
    CHECK_EQ(selectRange(source.sound, 2000, 33000).status, DecodeStatus::kInvalidRange);

    // Millisecond boundaries map to sample positions and preserve the exclusive end.
    DecodedSound indexed;
    indexed.samples.resize(144);
    for (std::size_t i = 0; i < indexed.samples.size(); ++i) indexed.samples[i] = static_cast<std::int16_t>(i);
    segment = selectRange(indexed, 1, 2);
    CHECK_EQ(segment.status, DecodeStatus::kOk);
    CHECK_EQ(segment.sound.lengthSamples(), 48u);
    CHECK_EQ(segment.sound.samples.front(), 48);
    CHECK_EQ(segment.sound.samples.back(), 95);

    const std::string oversized_path = tempPath("oversized.wav");
    writeWavFilled(oversized_path, 48000 * 301);
    CHECK_EQ(decoder.decodeFile(oversized_path, kMaxSourceSoundSamples).status,
             DecodeStatus::kTooLong);

    fs::remove(wav_path);
    fs::remove(bad_path);
    fs::remove(long_path);
    fs::remove(oversized_path);
}

TEST_MAIN()
