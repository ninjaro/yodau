#include "debug/telemetry_records.hpp"

#include <array>

namespace yodau::observability {
namespace {

const std::array catalog {
    metric_descriptor {
        "process_memory_rss_bytes", "Process RSS (measured)", "memory",
        "measured", "bytes", "process", "stock", "stable",
        "non_additive_system_total", "exact", "primary", "system.process",
    },
    metric_descriptor {
        "configured_stream_count", "Configured streams", "count",
        "accounted", "count", "application", "stock", "stable",
        "additive_within_scope", "exact", "secondary", "yodau.streams",
    },
    metric_descriptor {
        "visible_stream_count", "Visible streams", "count", "accounted",
        "count", "application", "stock", "recent_window",
        "additive_within_scope", "exact", "secondary", "yodau.streams",
    },
    metric_descriptor {
        "active_stream_count", "Active streams", "count", "accounted",
        "count", "application", "state", "stable", "non_additive",
        "exact", "secondary", "yodau.streams",
    },
    metric_descriptor {
        "configured_line_count", "Configured lines", "count", "accounted",
        "count", "application", "stock", "stable",
        "additive_within_scope", "exact", "secondary", "yodau.streams",
    },
    metric_descriptor {
        "detected_local_source_count", "Detected local sources", "count",
        "measured", "count", "application", "stock", "recent_window",
        "additive_within_scope", "exact", "secondary", "yodau.streams",
    },
    metric_descriptor {
        "tripwire_event_count", "Tripwire events", "count", "measured",
        "count", "session", "stock", "monotonic", "additive_within_scope",
        "exact", "secondary", "yodau.analysis",
    },
    metric_descriptor {
        "motion_event_count", "Motion events", "count", "measured", "count",
        "session", "stock", "monotonic", "additive_within_scope", "exact",
        "secondary", "yodau.analysis",
    },
    metric_descriptor {
        "frame_processing_time_ms", "Frame processing time", "duration",
        "measured", "milliseconds", "application", "state",
        "recent_window", "non_additive", "estimated", "primary",
        "yodau.analysis",
    },
    metric_descriptor {
        "input_fps", "Observed input FPS", "rate", "measured",
        "frames_per_second", "application", "state", "recent_window",
        "non_additive", "estimated", "primary", "yodau.streams",
    },
    metric_descriptor {
        "processing_fps", "Effective processing FPS", "rate", "measured",
        "frames_per_second", "application", "state", "recent_window",
        "non_additive", "estimated", "primary", "yodau.analysis",
    },
    metric_descriptor {
        "configured_processing_fps", "Configured processing FPS", "rate",
        "accounted", "frames_per_second", "application", "state", "stable",
        "non_additive", "exact", "secondary", "yodau.analysis",
    },
    metric_descriptor {
        "configured_display_fps", "Configured display FPS", "rate",
        "accounted", "frames_per_second", "application", "state", "stable",
        "non_additive", "exact", "secondary", "yodau.streams",
    },
    metric_descriptor {
        "dropped_gui_frame_count", "Superseded GUI frames", "count",
        "measured", "count", "session", "stock", "monotonic",
        "additive_within_scope", "exact", "secondary", "yodau.streams",
    },
    metric_descriptor {
        "monitor_queue_bytes", "Monitor producer queue", "memory", "measured",
        "bytes", "process", "state", "recent_window", "non_additive",
        "exact", "secondary", "yodau.observability",
    },
    metric_descriptor {
        "monitor_dropped_packet_count", "Dropped telemetry packets", "count",
        "measured", "count", "session", "stock", "monotonic",
        "additive_within_scope", "exact", "secondary",
        "yodau.observability",
    },
    metric_descriptor {
        "monitor_write_error_count", "Telemetry write errors", "count",
        "measured", "count", "session", "stock", "monotonic",
        "additive_within_scope", "exact", "secondary",
        "yodau.observability",
    },
};

} // namespace

std::string_view family_token(const message_family family) noexcept {
    switch (family) {
    case message_family::hello:
        return "hello";
    case message_family::sample_batch:
        return "sample_batch";
    case message_family::event_batch:
        return "event_batch";
    case message_family::snapshot:
        return "snapshot";
    case message_family::marker:
        return "marker";
    case message_family::warning:
        return "warning";
    case message_family::goodbye:
        return "goodbye";
    }
    return {};
}

std::string_view event_kind_token(const runtime_event_kind kind) noexcept {
    switch (kind) {
    case runtime_event_kind::motion:
        return "motion";
    case runtime_event_kind::tripwire:
        return "tripwire";
    case runtime_event_kind::roi:
        return "roi";
    case runtime_event_kind::info:
        return "info";
    }
    return "info";
}

std::span<const metric_descriptor> metric_catalog() noexcept {
    return catalog;
}

} // namespace yodau::observability
