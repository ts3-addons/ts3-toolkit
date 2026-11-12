#include <filesystem>
#include <fstream>
#include <string>

#include "tests/test_macros.h"
#include "ts3toolkit/config/config_store.h"

using namespace ts3toolkit::config;
namespace fs = std::filesystem;

namespace {

std::string pathFor(const std::string& name) {
    fs::path path = fs::temp_directory_path() /
                    ("ts3toolkit_" + name + ".cfg");
    return path.string();
}

void writeRaw(const std::string& path, const std::string& content) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << content;
}

}  // namespace

void runTests() {
    const std::string path = pathFor("store");
    fs::remove(path);
    fs::remove(path + ".tmp");

    // Missing file yields defaults.
    ConfigStore store(path);
    LoadOutcome first = store.load();
    CHECK_EQ(first.status, LoadStatus::kCreatedDefaults);
    CHECK_EQ(first.document.schema_version, kCurrentSchemaVersion);
    CHECK_TRUE(first.document.entrance_sound.enabled);

    // Round trip with non-default values.
    SettingsDocument document;
    document.entrance_sound.enabled = false;
    document.entrance_sound.sound_file = "C:\\sounds\\my clip.wav";
    document.entrance_sound.volume = 0.35f;
    document.entrance_sound.cooldown_seconds = 42;
    document.entrance_sound.local_monitor = true;
    CHECK_TRUE(store.save(document));
    CHECK_TRUE(!fs::exists(path + ".tmp"));

    LoadOutcome second = store.load();
    CHECK_EQ(second.status, LoadStatus::kLoaded);
    CHECK_EQ(second.document.entrance_sound.enabled, false);
    CHECK_EQ(second.document.entrance_sound.sound_file,
              std::string("C:\\sounds\\my clip.wav"));
    CHECK_CLOSE(second.document.entrance_sound.volume, 0.35f, 0.0001);
    CHECK_EQ(second.document.entrance_sound.cooldown_seconds, 42);
    CHECK_EQ(second.document.entrance_sound.local_monitor, true);

    // Corrupt content is rejected, not partially applied.
    writeRaw(path, "this is not valid config\n");
    LoadOutcome third = store.load();
    CHECK_EQ(third.status, LoadStatus::kRejected);
    CHECK_TRUE(third.document.entrance_sound.enabled);  // defaults retained

    // Out-of-range value is rejected.
    writeRaw(path, "schema_version = 1\n[entrance_sound]\nvolume = 2.0\n");
    CHECK_EQ(store.load().status, LoadStatus::kRejected);

    // Unknown key is rejected.
    writeRaw(path, "schema_version = 1\n[entrance_sound]\nunknown = 1\n");
    CHECK_EQ(store.load().status, LoadStatus::kRejected);

    // Future schema is rejected.
    writeRaw(path, "schema_version = 99\n[entrance_sound]\n");
    CHECK_EQ(store.load().status, LoadStatus::kRejected);

    // Saving an invalid document fails without touching the file.
    SettingsDocument invalid = document;
    invalid.entrance_sound.volume = 5.0f;
    CHECK_TRUE(!store.save(invalid));

    fs::remove(path);
    fs::remove(path + ".tmp");
}

TEST_MAIN()
