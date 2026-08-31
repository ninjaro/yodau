#ifndef YODAU_CORE_EVENT_HPP
#define YODAU_CORE_EVENT_HPP
#include <chrono>
#include <optional>
#include <string>

#include "geometry/geometry.hpp"

namespace yodau::core {
enum class event_kind { motion, tripwire, roi, info };

struct event {
    event_kind kind { event_kind::info };
    std::string stream_name;
    std::string message;
    std::chrono::steady_clock::time_point ts;

    std::optional<point> pos_pct;
    std::string line_name;
};
}
#endif // YODAU_CORE_EVENT_HPP
