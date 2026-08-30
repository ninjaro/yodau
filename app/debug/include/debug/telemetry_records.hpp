#ifndef YODAU_DEBUG_TELEMETRY_RECORDS_HPP
#define YODAU_DEBUG_TELEMETRY_RECORDS_HPP

#include "debug/runtime_observer.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace yodau::observability {

enum class message_family {
    hello,
    sample_batch,
    event_batch,
    snapshot,
    marker,
    warning,
    goodbye,
};

[[nodiscard]] std::string_view family_token(message_family family) noexcept;
[[nodiscard]] std::string_view
event_kind_token(runtime_event_kind kind) noexcept;

struct producer_identity {
    std::string app;
    std::int64_t pid = -1;
    std::string build;
    std::vector<std::string> debug_flags;
    std::string instrumentation_mode;
};

struct metric_descriptor {
    std::string id;
    std::string label;
    std::string kind;
    std::string provenance;
    std::string unit;
    std::string scope;
    std::string cardinality_semantics;
    std::string stability;
    std::string additive_semantics;
    std::string confidence;
    std::string default_display_role;
    std::string domain_namespace;
};

[[nodiscard]] std::span<const metric_descriptor> metric_catalog() noexcept;

using numeric_value = std::variant<std::int64_t, double>;

struct numeric_sample {
    std::string metric_id;
    numeric_value value { std::int64_t { 0 } };
};

struct record_header {
    message_family family { message_family::sample_batch };
    std::string session;
    std::int64_t monotonic_timestamp_ms = 0;
};

struct hello_record {
    record_header header { message_family::hello, {}, 0 };
    producer_identity identity;
    std::vector<metric_descriptor> metrics;
    std::vector<std::string> extensions;
};

struct sample_batch_record {
    record_header header { message_family::sample_batch, {}, 0 };
    std::vector<numeric_sample> samples;
};

struct event_record {
    std::int64_t collector_sequence = 0;
    runtime_event_kind kind { runtime_event_kind::info };
    std::int64_t timestamp_ms = 0;
    std::string stream_name;
    std::string line_name;
    std::string message;
    std::optional<double> position_x_pct;
    std::optional<double> position_y_pct;
};

struct event_batch_record {
    record_header header { message_family::event_batch, {}, 0 };
    std::vector<event_record> events;
};

struct snapshot_record {
    record_header header { message_family::snapshot, {}, 0 };
    std::vector<numeric_sample> samples;
    std::string reason;
    bool listener_connected = false;
    std::string endpoint_path;
};

struct marker_record {
    record_header header { message_family::marker, {}, 0 };
    std::string label;
};

struct warning_record {
    record_header header { message_family::warning, {}, 0 };
    std::string warning_code;
    std::string warning_message;
};

struct goodbye_record {
    record_header header { message_family::goodbye, {}, 0 };
    std::string final_endpoint_path;
};

} // namespace yodau::observability

#endif // YODAU_DEBUG_TELEMETRY_RECORDS_HPP
