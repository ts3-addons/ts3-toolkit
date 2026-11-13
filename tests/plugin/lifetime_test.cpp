#include <type_traits>
#include <vector>

#include "src/plugin/plugin_lifetime.h"
#include "tests/test_macros.h"

namespace {

struct Client {
    std::vector<int>& events;
    ~Client() { events.push_back(2); }
};

struct Runtime {
    std::vector<int>& events;
    ~Runtime() { events.push_back(1); }
};

using Lifetime = ts3toolkit::plugin::PluginLifetime<Client, Runtime>;
static_assert(std::is_trivially_destructible_v<Lifetime>);
static_assert(!std::is_copy_constructible_v<Lifetime>);

}  // namespace

void runTests() {
    std::vector<int> events;
    Lifetime lifetime;
    lifetime.client = new Client{events};
    lifetime.runtime = new Runtime{events};
    lifetime.shutdown();
    CHECK_EQ(events.size(), 2u);
    CHECK_EQ(events[0], 1);
    CHECK_EQ(events[1], 2);
    CHECK_TRUE(lifetime.runtime == nullptr);
    CHECK_TRUE(lifetime.client == nullptr);
    lifetime.shutdown();
    CHECK_EQ(events.size(), 2u);

    lifetime.client = new Client{events};
    lifetime.runtime = new Runtime{events};
    lifetime.resetRuntime();
    CHECK_EQ(events.size(), 3u);
    CHECK_EQ(events[2], 1);
    CHECK_TRUE(lifetime.client != nullptr);
    lifetime.shutdown();
    CHECK_EQ(events.size(), 4u);
    CHECK_EQ(events[3], 2);
}

TEST_MAIN()
