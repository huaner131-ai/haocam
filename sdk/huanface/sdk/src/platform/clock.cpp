/**
 * HuanFace Platform Clock — Phase 3
 * Cross-platform using std::chrono, Windows QPC
 */

#include <chrono>
#include <cstdint>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace huanface {

class IClock {
public:
    virtual ~IClock() = default;
    virtual int64_t NowNanos() = 0;
    virtual double NowMillis() = 0;
    virtual double NowSeconds() = 0;
    virtual void SleepMillis(int millis) = 0;
};

class StdClock : public IClock {
public:
    int64_t NowNanos() override {
        auto now = std::chrono::high_resolution_clock::now();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
        return nanos;
    }
    double NowMillis() override {
        return NowNanos() / 1e6;
    }
    double NowSeconds() override {
        return NowNanos() / 1e9;
    }
    void SleepMillis(int millis) override {
        std::this_thread::sleep_for(std::chrono::milliseconds(millis));
    }
};

#ifdef _WIN32
class WindowsClock : public IClock {
public:
    WindowsClock() {
        QueryPerformanceFrequency(&freq);
    }
    int64_t NowNanos() override {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        int64_t nanos = (counter.QuadPart * 1000000000LL) / freq.QuadPart;
        return nanos;
    }
    double NowMillis() override { return NowNanos() / 1e6; }
    double NowSeconds() override { return NowNanos() / 1e9; }
    void SleepMillis(int millis) override { Sleep(millis); }
private:
    LARGE_INTEGER freq;
};
#endif

} // namespace huanface
