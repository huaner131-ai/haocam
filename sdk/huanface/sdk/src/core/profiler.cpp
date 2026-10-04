/**
 * Profiler Implementation — Phase 8
 */

#include "profiler.h"

namespace huanface {

Profiler& GetGlobalProfiler() {
    static Profiler instance;
    return instance;
}

} // namespace huanface
