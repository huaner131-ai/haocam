#include <vector>

#include "frame/FramePool.h"
#include "frame/TexturePool.h"
#include "test_main.h"

using haocam::FrameBufferPool;

HAOCAM_TEST(frame_buffer_pool_allocates_and_recycles) {
    FrameBufferPool pool(4);

    {
        auto buffer = pool.acquireNV12(640, 480);
        HAOCAM_EXPECT(buffer != nullptr);
        HAOCAM_EXPECT_EQ(buffer->width, 640u);
        HAOCAM_EXPECT_EQ(buffer->height, 480u);
        HAOCAM_EXPECT_EQ(buffer->data.size(),
                         static_cast<size_t>(640) * 480 * 3 / 2);
        HAOCAM_EXPECT_EQ(pool.idleCount(), 0u);
    }
    // Released -> returned to the pool.
    HAOCAM_EXPECT_EQ(pool.idleCount(), 1u);

    {
        auto again = pool.acquireNV12(640, 480);
        HAOCAM_EXPECT(again != nullptr);
        HAOCAM_EXPECT_EQ(pool.idleCount(), 0u);

        auto other = pool.acquireNV12(1280, 720); // different size -> new buffer
        HAOCAM_EXPECT(other != nullptr);
        HAOCAM_EXPECT_EQ(other->width, 1280u);
    }
    HAOCAM_EXPECT_EQ(pool.idleCount(), 2u);
}

HAOCAM_TEST(frame_buffer_pool_caps_idle_buffers) {
    FrameBufferPool pool(1);
    {
        auto a = pool.acquireNV12(64, 64);
        auto b = pool.acquireNV12(64, 64);
        HAOCAM_EXPECT(a != b);
    }
    HAOCAM_EXPECT_EQ(pool.idleCount(), 1u); // only one kept alive
}

HAOCAM_TEST(frame_pool_assigns_monotonic_ids) {
    haocam::FramePool pool(100);
    auto shellA = pool.createShell(1920, 1080, haocam::PixelFormat::NV12);
    auto shellB = pool.createShell(1920, 1080, haocam::PixelFormat::NV12);
    HAOCAM_EXPECT_EQ(shellA.id, 100u);
    HAOCAM_EXPECT_EQ(shellB.id, 101u);
    HAOCAM_EXPECT_EQ(pool.lastId(), 101u);
}
