#ifndef YODAU_DEBUG_RUNTIME_OBSERVER_HPP
#define YODAU_DEBUG_RUNTIME_OBSERVER_HPP

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace yodau::observability {

enum class runtime_event_kind {
    info,
    motion,
    tripwire,
    roi,
};

struct runtime_event_view {
    runtime_event_kind kind = runtime_event_kind::info;
    std::chrono::steady_clock::time_point timestamp {};
    std::string_view stream_name;
    std::string_view line_name;
    std::string_view message;
    std::optional<double> position_x_pct;
    std::optional<double> position_y_pct;
};

struct inventory_statistics {
    std::int64_t configured_streams = 0;
    std::int64_t visible_streams = 0;
    std::int64_t active_streams = 0;
    std::int64_t configured_lines = 0;
    std::int64_t detected_local_sources = 0;
};

struct processing_statistics {
    double frame_processing_time_ms = 0.0;
    double input_fps = 0.0;
    double processing_fps = 0.0;
    double configured_processing_fps = 0.0;
    double configured_display_fps = 0.0;
    std::uint64_t dropped_gui_frames = 0;
};

// The processing layer owns the statistics and event lifetimes. Implementations
// must copy any borrowed event text they retain after record_events returns.
// Implementations must not perform serialization or socket I/O from the update
// methods; recurring publication belongs to their background cadence.
class runtime_observer {
public:
    virtual ~runtime_observer() = default;

    virtual void update_inventory(const inventory_statistics& value) noexcept
        = 0;
    virtual void update_processing(const processing_statistics& value) noexcept
        = 0;
    [[nodiscard]] virtual bool wants_event_details() const noexcept = 0;
    virtual void record_events(std::span<const runtime_event_view> events) = 0;
    virtual void add_marker(std::string_view label) = 0;
};

} // namespace yodau::observability

#endif // YODAU_DEBUG_RUNTIME_OBSERVER_HPP
