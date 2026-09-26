#include <string>

#include "core/events/EventBus.h"
#include "test_main.h"

namespace {
struct TestEvent {
    int value = 0;
    std::string label;
};
} // namespace

HAOCAM_TEST(eventbus_delivers_to_all_subscribers) {
    auto& bus = haocam::core::EventBus::instance();
    int sum = 0;
    std::string last;

    const auto tokenA =
        bus.subscribe<TestEvent>([&](const TestEvent& e) { sum += e.value; last = e.label; });
    const auto tokenB =
        bus.subscribe<TestEvent>([&](const TestEvent& e) { sum += e.value * 10; });

    bus.publish(TestEvent{2, "hello"});
    HAOCAM_EXPECT_EQ(sum, 22);
    HAOCAM_EXPECT_EQ(last, "hello");

    bus.unsubscribe(tokenA);
    bus.unsubscribe(tokenB);
    bus.publish(TestEvent{5, "x"});
    HAOCAM_EXPECT_EQ(sum, 22); // unchanged after unsubscribe
}

HAOCAM_TEST(eventbus_unsubscribe_of_unknown_token_is_safe) {
    auto& bus = haocam::core::EventBus::instance();
    bus.unsubscribe(999999); // must not crash
}
