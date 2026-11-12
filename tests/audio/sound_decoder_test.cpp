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

    // Header claiming more than the 30 second limit.
    const std::string long_path = tempPath("long.wav");
    writeWav(long_path, samples, 48000 * 31);
    DecodeResult too_long = decoder.decodeFile(long_path);
    CHECK_EQ(too_long.status, DecodeStatus::kTooLong);

    fs::remove(wav_path);
    fs::remove(bad_path);
    fs::remove(long_path);
}

TEST_MAIN()
