#pragma once

// Minimal assert-based test harness (no external dependencies).

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace testfw {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

inline int& failureCount() {
    static int count = 0;
    return count;
}

inline bool registerTest(const std::string& name, std::function<void()> fn) {
    registry().push_back({name, std::move(fn)});
    return true;
}

inline int runAll() {
    for (const auto& test : registry()) {
        const int before = failureCount();
        test.fn();
        std::printf("[%s] %s\n", failureCount() == before ? "PASS" : "FAIL",
                    test.name.c_str());
    }
    std::printf("%zu test(s), %d failure(s)\n", registry().size(), failureCount());
    return failureCount() == 0 ? 0 : 1;
}

} // namespace testfw

#define HAOCAM_TEST(name)                                                       \
    static void name##_impl();                                                  \
    static const bool name##_registered =                                       \
        ::testfw::registerTest(#name, name##_impl);                             \
    static void name##_impl()

#define HAOCAM_EXPECT(cond)                                                     \
    do {                                                                        \
        if (!(cond)) {                                                          \
            ++::testfw::failureCount();                                         \
            std::printf("  EXPECT failed at %s:%d: %s\n", __FILE__, __LINE__,   \
                        #cond);                                                 \
        }                                                                       \
    } while (0)

#define HAOCAM_EXPECT_EQ(a, b)                                                  \
    do {                                                                        \
        const auto va = (a); /* by value: temporaries must not dangle */        \
        const auto vb = (b);                                                    \
        if (!(va == vb)) {                                                      \
            ++::testfw::failureCount();                                         \
            std::printf("  EXPECT_EQ failed at %s:%d: %s == %s\n", __FILE__,    \
                        __LINE__, #a, #b);                                      \
        }                                                                       \
    } while (0)
