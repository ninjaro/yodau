#include "monitor/debug_build_gate.hpp"

bool yodau::monitoring::debug_monitor_compile_time_enabled() {
    return YODAU_DEBUG_OBSERVABILITY != 0;
}
