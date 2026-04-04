#include "streams/analysis_scheduler.hpp"

namespace yodau::core {

void analysis_scheduler::set_default_interval_ms(const int ms) {
    if (ms <= 0) {
        return;
    }

    std::scoped_lock lock(mtx);
    default_interval_ms = ms;
}

void analysis_scheduler::set_stream_interval_ms(
    const std::string& stream_name, const int ms
) {
    if (stream_name.empty() || ms <= 0) {
        return;
    }

    std::scoped_lock lock(mtx);
    interval_overrides_ms[stream_name] = ms;
}

void analysis_scheduler::clear_stream_interval_ms(
    const std::string& stream_name
) {
    if (stream_name.empty()) {
        return;
    }

    std::scoped_lock lock(mtx);
    interval_overrides_ms.erase(stream_name);
}

int analysis_scheduler::interval_for_stream(const std::string& stream_name) const {
    std::scoped_lock lock(mtx);
    const auto interval_it = interval_overrides_ms.find(stream_name);
    if (interval_it != interval_overrides_ms.end() && interval_it->second > 0) {
        return interval_it->second;
    }

    return default_interval_ms;
}

bool analysis_scheduler::should_process(
    const std::string& stream_name, const time_point now
) {
    if (stream_name.empty()) {
        return false;
    }

    std::scoped_lock lock(mtx);

    const auto interval_it = interval_overrides_ms.find(stream_name);
    const int interval_ms = interval_it != interval_overrides_ms.end()
            && interval_it->second > 0
        ? interval_it->second
        : default_interval_ms;

    const auto last_it = last_analysis_ts.find(stream_name);
    if (last_it == last_analysis_ts.end()) {
        last_analysis_ts[stream_name] = now;
        return true;
    }

    const auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - last_it->second
                    )
                        .count();
    if (dt < interval_ms) {
        return false;
    }

    last_analysis_ts[stream_name] = now;
    return true;
}

} // namespace yodau::core
