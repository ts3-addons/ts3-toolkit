#include "tests/test_macros.h"
#include "ts3toolkit/plugin/client_api.h"
#include "ts3toolkit/plugin/talk_state_manager.h"

using namespace ts3toolkit;

namespace {

// Records transmission settings and call counts so TalkStateManager can be
// driven without the real client. Individual operations can be made to fail.
class FakeClientApi : public plugin::ClientApi {
public:
    bool voice_activation = false;
    bool input_deactivated = true;  // default: push-to-talk style

    int flush_count = 0;
    bool fail_get = false;
    bool fail_set_vad = false;
    bool fail_set_input = false;
    bool fail_flush = false;

    bool registerCustomDevice(const std::string&, const std::string&, int,
                              int) override {
        return true;
    }
    bool unregisterCustomDevice(const std::string&) override {
        return true;
    }
    bool pushCaptureFrames(const std::string&, const core::Sample*,
                           int) override {
        return true;
    }

    bool getLocalClientId(uint64_t, uint16_t&) override {
        return true;
    }
    bool getChannelOfClient(uint64_t, uint16_t, uint64_t&) override {
        return true;
    }
    uint64_t currentConnectionId() override {
        return 1;
    }

    bool getVoiceActivationEnabled(uint64_t, bool& enabled) override {
        if (fail_get) {
            return false;
        }
        enabled = voice_activation;
        return true;
    }
    bool getInputDeactivated(uint64_t, bool& deactivated) override {
        if (fail_get) {
            return false;
        }
        deactivated = input_deactivated;
        return true;
    }
    bool setVoiceActivationEnabled(uint64_t, bool enabled) override {
        if (fail_set_vad) {
            return false;
        }
        voice_activation = enabled;
        return true;
    }
    bool setInputDeactivated(uint64_t, bool deactivated) override {
        if (fail_set_input) {
            return false;
        }
        input_deactivated = deactivated;
        return true;
    }
    bool flushSelfUpdates(uint64_t) override {
        if (fail_flush) {
            return false;
        }
        ++flush_count;
        return true;
    }

    std::string configPath() override {
        return "config";
    }
    std::string pluginPath() override {
        return "plugin";
    }

    void log(const std::string&, plugin::LogSeverity) override {}
};

}  // namespace

void runTests() {
    constexpr uint64_t kConnection = 1;

    // beginForced lifts push-to-talk gating into continuous transmission.
    FakeClientApi client;
    plugin::TalkStateManager manager(client);
    CHECK_EQ(manager.isForcing(), false);

    CHECK_TRUE(manager.beginForced(kConnection));
    CHECK_EQ(manager.isForcing(), true);
    CHECK_EQ(client.voice_activation, false);
    CHECK_EQ(client.input_deactivated, false);
    CHECK_EQ(client.flush_count, 1);

    // A second begin while already forcing is rejected.
    CHECK_TRUE(!manager.beginForced(kConnection));

    // endForced restores the original PTT configuration.
    CHECK_TRUE(manager.endForced(kConnection));
    CHECK_EQ(manager.isForcing(), false);
    CHECK_EQ(client.voice_activation, false);
    CHECK_EQ(client.input_deactivated, true);
    CHECK_EQ(client.flush_count, 2);

    // endForced without an active force is a no-op.
    CHECK_TRUE(!manager.endForced(kConnection));

    // Voice activation state is also preserved across a force cycle.
    FakeClientApi va_client;
    va_client.voice_activation = true;
    va_client.input_deactivated = false;
    plugin::TalkStateManager va_manager(va_client);
    CHECK_TRUE(va_manager.beginForced(kConnection));
    CHECK_EQ(va_client.voice_activation, false);
    CHECK_EQ(va_client.input_deactivated, false);
    CHECK_TRUE(va_manager.endForced(kConnection));
    CHECK_EQ(va_client.voice_activation, true);
    CHECK_EQ(va_client.input_deactivated, false);

    // A read failure aborts without changing anything.
    FakeClientApi read_fail;
    read_fail.fail_get = true;
    plugin::TalkStateManager read_manager(read_fail);
    CHECK_TRUE(!read_manager.beginForced(kConnection));
    CHECK_EQ(read_manager.isForcing(), false);
    CHECK_EQ(read_fail.input_deactivated, true);

    // A write failure rolls back to the snapshot and does not force.
    FakeClientApi write_fail;
    write_fail.fail_set_input = true;
    plugin::TalkStateManager write_manager(write_fail);
    CHECK_TRUE(!write_manager.beginForced(kConnection));
    CHECK_EQ(write_manager.isForcing(), false);
    CHECK_EQ(write_fail.input_deactivated, true);
}

TEST_MAIN()
