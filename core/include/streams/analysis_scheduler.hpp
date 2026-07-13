#ifndef YODAU_CORE_ANALYSIS_SCHEDULER_HPP
#define YODAU_CORE_ANALYSIS_SCHEDULER_HPP

#include "core/namespace_alias.hpp"
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

namespace yodau::core {

class analysis_scheduler {
public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    void set_default_interval_ms(int ms);
    void set_stream_interval_ms(const std::string& stream_name, int ms);
    void clear_stream_interval_ms(const std::string& stream_name);
    void remove_stream(const std::string& stream_name);

    int interval_for_stream(const std::string& stream_name) const;
    bool should_process(
        const std::string& stream_name, time_point now = clock::now()
    );

private:
    int default_interval_ms { 200 };
    std::unordered_map<std::string, int> interval_overrides_ms;
    std::unordered_map<std::string, time_point> last_analysis_ts;
    mutable std::mutex mtx;
};

} // namespace yodau::core

#endif // YODAU_CORE_ANALYSIS_SCHEDULER_HPP
