#include "monitor/debug_build_gate.hpp"

bool yodau::monitoring::debug_monitor_compile_time_enabled() {
#if defined(NDEBUG)
    return false;
#else
    return true;
#endif
}
