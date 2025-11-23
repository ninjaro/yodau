#ifndef YODAU_BACKEND_EVENT_HPP
#define YODAU_BACKEND_EVENT_HPP
#include <chrono>
#include <optional>
#include <string>

#include "geometry.hpp"

namespace yodau::backend {
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
#endif // YODAU_BACKEND_EVENT_HPP