#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

#include "frame/Frame.h"

namespace haocam {

// Allocates monotonically increasing frame ids and produces Frame shells.
// Purely bookkeeping; GPU memory lives in the texture pool.
class FramePool {
public:
    explicit FramePool(uint64_t firstId = 1) : m_nextId(firstId) {}

    Frame createShell(uint32_t width, uint32_t height, PixelFormat format) {
        std::lock_guard<std::mutex> lock(m_mutex);
        Frame frame;
        frame.id = m_nextId++;
        frame.width = width;
        frame.height = height;
        frame.format = format;
        return frame;
    }

    uint64_t lastId() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_nextId - 1;
    }

private:
    mutable std::mutex m_mutex;
    uint64_t m_nextId;
};

} // namespace haocam
