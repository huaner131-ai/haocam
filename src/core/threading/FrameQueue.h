#pragma once

// Single-producer / single-consumer frame queue with bounded capacity.
//
// Used to hand camera frames from the capture thread to the render thread.
// The queue never blocks the producer for long: when full, it either drops
// the oldest frame (default, keeps latency low for a live camera pipeline)
// or rejects the newest one. Consumer may block with a timeout.

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace haocam::core {

template <typename T>
class FrameQueue {
public:
    explicit FrameQueue(size_t capacity = 3)
        : m_slots(capacity > 0 ? capacity : 1) {}

    // Non-blocking. Returns false when the queue is full in `rejectNew` mode.
    bool push(T&& item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_size == m_slots.size()) {
            if (m_rejectNew) return false;
            // Drop the oldest frame to make room (keeps live latency bounded).
            m_slots[m_head] = std::move(item);
            m_head = (m_head + 1) % m_slots.size();
            ++m_dropped;
        } else {
            m_slots[(m_head + m_size) % m_slots.size()] = std::move(item);
            ++m_size;
        }
        m_cv.notify_one();
        return true;
    }
    bool push(const T& item) { return push(T(item)); }

    // Blocks up to `timeout`. Returns nullopt on timeout.
    template <typename Rep, typename Period>
    std::optional<T> pop(std::chrono::duration<Rep, Period> timeout) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!m_cv.wait_for(lock, timeout, [&] { return m_size > 0; })) return std::nullopt;
        return popLocked();
    }

    std::optional<T> tryPop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_size == 0) return std::nullopt;
        return popLocked();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_size;
    }

    uint64_t droppedCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_dropped;
    }

    void setRejectNew(bool rejectNew) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_rejectNew = rejectNew;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_size = 0;
        m_head = 0;
    }

private:
    T popLocked() {
        T item = std::move(m_slots[m_head]);
        m_head = (m_head + 1) % m_slots.size();
        --m_size;
        return item;
    }

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::vector<T> m_slots;
    size_t m_head = 0;
    size_t m_size = 0;
    uint64_t m_dropped = 0;
    bool m_rejectNew = false;
};

} // namespace haocam::core
