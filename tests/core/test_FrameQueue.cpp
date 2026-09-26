#include <atomic>
#include <thread>
#include <vector>

#include "core/threading/FrameQueue.h"
#include "test_main.h"

using haocam::core::FrameQueue;

HAOCAM_TEST(frame_queue_fifo) {
    FrameQueue<int> queue(4);
    for (int i = 0; i < 4; ++i) {
        HAOCAM_EXPECT(queue.push(i));
    }
    HAOCAM_EXPECT_EQ(queue.size(), 4u);
    for (int i = 0; i < 4; ++i) {
        const auto value = queue.tryPop();
        HAOCAM_EXPECT(value.has_value());
        HAOCAM_EXPECT_EQ(*value, i);
    }
    HAOCAM_EXPECT(!queue.tryPop().has_value());
}

HAOCAM_TEST(frame_queue_drop_oldest_when_full) {
    FrameQueue<int> queue(2);
    queue.push(1);
    queue.push(2);
    queue.push(3); // overwrites the oldest
    queue.push(4);
    HAOCAM_EXPECT_EQ(queue.size(), 2u);
    HAOCAM_EXPECT(queue.droppedCount() >= 2);
    HAOCAM_EXPECT_EQ(*queue.tryPop(), 3);
    HAOCAM_EXPECT_EQ(*queue.tryPop(), 4);
}

HAOCAM_TEST(frame_queue_spsc_stress) {
    FrameQueue<uint64_t> queue(8);
    std::atomic<bool> done{false};
    std::atomic<uint64_t> sum{0};

    std::thread producer([&] {
        for (uint64_t i = 1; i <= 50000; ++i) {
            queue.push(i);
        }
        done = true;
    });
    std::thread consumer([&] {
        uint64_t localSum = 0;
        while (!done.load() || queue.size() > 0) {
            if (auto value = queue.tryPop()) {
                localSum += *value;
            }
        }
        sum = localSum;
    });

    producer.join();
    consumer.join();

    // Values may be dropped under saturation, but the consumer must never
    // deadlock or see corrupted slots. Sum of received values must be > 0.
    HAOCAM_EXPECT(sum.load() > 0);
}

HAOCAM_TEST(frame_queue_pop_with_timeout) {
    FrameQueue<int> queue(2);
    const auto value = queue.pop(std::chrono::milliseconds(10));
    HAOCAM_EXPECT(!value.has_value());
    queue.push(7);
    const auto value2 = queue.pop(std::chrono::milliseconds(10));
    HAOCAM_EXPECT(value2.has_value());
    HAOCAM_EXPECT_EQ(*value2, 7);
}
