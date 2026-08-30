#include "monitor/runtime_bridge.hpp"

#include "monitor/debug_broadcaster.hpp"
#include "monitor/telemetry_json.hpp"
#include "monitor/process_memory_reader.hpp"
#include "monitor/runtime_build_info.hpp"

#include <QCoreApplication>
#include <QTimer>
#include <QUuid>

#include <QDebug>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

namespace yodau::monitor {
namespace {

constexpr std::size_t maximum_events_per_batch = 4096U;
constexpr std::size_t maximum_event_text_bytes = 4096U;
constexpr std::size_t maximum_marker_bytes = 4096U;

std::string bounded_text(
    const std::string_view value, const std::size_t maximum_bytes
) {
    return std::string(value.substr(0U, maximum_bytes));
}

} // namespace

runtime_bridge::runtime_bridge(QObject* parent)
    : runtime_bridge(runtime_options(), parent) { }

runtime_bridge::runtime_bridge(
    runtime_options requested_options, QObject* parent
)
    : QObject(parent)
    , options(std::move(requested_options))
    , broadcaster(new yodau::monitoring::debug_broadcaster(this))
    , sample_timer(new QTimer(this))
    , state()
    , session_start_steady(std::chrono::steady_clock::now())
    , session_id(QUuid::createUuid().toString(QUuid::WithoutBraces))
    , event_sequence(0)
    , sample_tick_count(0)
    , goodbye_published(false) {
    sample_timer->setInterval(1000);
    sample_timer->setSingleShot(false);
    QObject::connect(
        sample_timer, &QTimer::timeout, this,
        &runtime_bridge::on_sample_timer_timeout
    );
    QObject::connect(
        broadcaster,
        &yodau::monitoring::debug_broadcaster::listener_connection_changed,
        this, [this](const bool connected) {
            sample_timer->stop();
            if (connected) {
                goodbye_published = false;
                publish_hello();
                publish_snapshot(QStringLiteral("listener_connected"));
                publish_sample_batch();
                sample_timer->start();
            }
            emit state_changed();
        }
    );
    QObject::connect(
        broadcaster, &yodau::monitoring::debug_broadcaster::warning_raised,
        this, &runtime_bridge::on_broadcaster_warning
    );
    if (QCoreApplication::instance() != nullptr) {
        QObject::connect(
            QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this,
            &runtime_bridge::on_about_to_quit
        );
    }

    if (options.enabled) {
        set_enabled(true, options.requested_endpoint);
    }
}

runtime_bridge::~runtime_bridge() {
    if (can_publish()) {
        publish_snapshot(QStringLiteral("destructor"));
        publish_goodbye();
    }
}

bool runtime_bridge::set_enabled(
    const bool enabled, const QString& requested_endpoint
) {
    if (broadcaster == nullptr) {
        return false;
    }

    const bool was_enabled = broadcaster->is_enabled();
    if (was_enabled && !enabled) {
        if (can_publish()) {
            publish_snapshot(QStringLiteral("disabled"));
            publish_goodbye();
        }
        sample_timer->stop();
    }

    const QString effective_endpoint = requested_endpoint.isEmpty()
        ? options.requested_endpoint
        : requested_endpoint;
    if (enabled && !effective_endpoint.isEmpty()) {
        options.requested_endpoint = effective_endpoint;
    }

    const bool changed = broadcaster->set_enabled(enabled, effective_endpoint);
    if (enabled && broadcaster->is_enabled()) {
        qInfo() << "yodau monitor endpoint:" << endpoint_path();
    }
    if (!enabled || !broadcaster->is_enabled()) {
        sample_timer->stop();
    }
    emit state_changed();
    return changed;
}

void runtime_bridge::update_inventory(
    const yodau::observability::inventory_statistics& value
) noexcept {
    state.inventory.configured_streams
        = std::max<std::int64_t>(0, value.configured_streams);
    state.inventory.visible_streams
        = std::max<std::int64_t>(0, value.visible_streams);
    state.inventory.active_streams
        = std::max<std::int64_t>(0, value.active_streams);
    state.inventory.configured_lines
        = std::max<std::int64_t>(0, value.configured_lines);
    state.inventory.detected_local_sources
        = std::max<std::int64_t>(0, value.detected_local_sources);
}

void runtime_bridge::update_processing(
    const yodau::observability::processing_statistics& value
) noexcept {
    state.processing.frame_processing_time_ms
        = std::max(0.0, value.frame_processing_time_ms);
    state.processing.input_fps = std::max(0.0, value.input_fps);
    state.processing.processing_fps = std::max(0.0, value.processing_fps);
    state.processing.configured_processing_fps
        = std::max(0.0, value.configured_processing_fps);
    state.processing.configured_display_fps
        = std::max(0.0, value.configured_display_fps);
    state.processing.dropped_gui_frames = std::min<std::uint64_t>(
        value.dropped_gui_frames,
        static_cast<std::uint64_t>(std::numeric_limits<qint64>::max())
    );
}

bool runtime_bridge::wants_event_details() const noexcept {
    return can_publish();
}

void runtime_bridge::record_events(
    const std::span<const yodau::observability::runtime_event_view> events
) {
    if (events.empty() || !can_publish()) {
        return;
    }

    yodau::observability::event_batch_record batch { .header = header(
        yodau::observability::message_family::event_batch
    ), .events = {} };
    const std::size_t retained_count
        = std::min(events.size(), maximum_events_per_batch);
    batch.events.reserve(retained_count);
    for (const auto& event : events.first(retained_count)) {
        if (event.kind == yodau::observability::runtime_event_kind::tripwire) {
            ++state.tripwire_event_count;
        } else if (
            event.kind == yodau::observability::runtime_event_kind::motion
        ) {
            ++state.motion_event_count;
        }

        batch.events.push_back(yodau::observability::event_record {
            .collector_sequence = ++event_sequence,
            .kind = event.kind,
            .timestamp_ms = event_timestamp_ms(event),
            .stream_name
            = bounded_text(event.stream_name, maximum_event_text_bytes),
            .line_name
            = bounded_text(event.line_name, maximum_event_text_bytes),
            .message = bounded_text(event.message, maximum_event_text_bytes),
            .position_x_pct = event.position_x_pct,
            .position_y_pct = event.position_y_pct,
        });
    }
    publish_message(
        yodau::data::encode_telemetry_json(batch),
        publish_priority::medium, true
    );
}

void runtime_bridge::add_marker(const std::string_view label) {
    if (label.empty() || !can_publish()) {
        return;
    }
    std::string trimmed = bounded_text(label, maximum_marker_bytes);
    const auto first = trimmed.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return;
    }
    const auto last = trimmed.find_last_not_of(" \t\r\n");
    trimmed = trimmed.substr(first, last - first + 1U);
    publish_message(
        yodau::data::encode_telemetry_json(
            yodau::observability::marker_record {
                .header = header(yodau::observability::message_family::marker),
                .label = std::move(trimmed),
            }
        ),
        publish_priority::medium, true
    );
}

void runtime_bridge::set_inventory(
    const int configured_streams, const int visible_streams,
    const int active_streams, const int configured_lines,
    const int detected_local_sources
) noexcept {
    update_inventory(yodau::observability::inventory_statistics {
        .configured_streams = configured_streams,
        .visible_streams = visible_streams,
        .active_streams = active_streams,
        .configured_lines = configured_lines,
        .detected_local_sources = detected_local_sources,
    });
}

bool runtime_bridge::is_enabled() const {
    return broadcaster != nullptr && broadcaster->is_enabled();
}

bool runtime_bridge::is_connected() const {
    return broadcaster != nullptr && broadcaster->has_listener();
}

QString runtime_bridge::endpoint_path() const {
    return broadcaster != nullptr ? broadcaster->state().endpoint_name
                                  : QString();
}

qint64 runtime_bridge::queued_bytes() const {
    return broadcaster != nullptr ? broadcaster->state().queued_bytes : 0;
}

qint64 runtime_bridge::published_message_count() const {
    return broadcaster != nullptr ? broadcaster->state().published_messages : 0;
}

void runtime_bridge::on_about_to_quit() {
    if (can_publish()) {
        sample_timer->stop();
        publish_snapshot(QStringLiteral("shutdown"));
        publish_goodbye();
    }
}

void runtime_bridge::on_sample_timer_timeout() {
    if (can_publish()) {
        publish_sample_batch();
    } else {
        sample_timer->stop();
    }
}

void runtime_bridge::on_broadcaster_warning(
    const QString& warning_code, const QString& warning_message
) {
    qWarning() << "yodau monitor warning:" << warning_code << warning_message;
    if (can_publish()) {
        publish_warning(warning_code, warning_message);
    }
    emit state_changed();
}

qint64 runtime_bridge::monotonic_timestamp_ms() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - session_start_steady
    )
        .count();
}

qint64 runtime_bridge::event_timestamp_ms(
    const yodau::observability::runtime_event_view& event
) const {
    if (event.timestamp.time_since_epoch().count() == 0) {
        return monotonic_timestamp_ms();
    }
    return std::max<qint64>(
        0,
        std::chrono::duration_cast<std::chrono::milliseconds>(
            event.timestamp - session_start_steady
        )
            .count()
    );
}

yodau::observability::producer_identity runtime_bridge::identity() const {
    yodau::observability::producer_identity info;
    info.app = "yodau";
    info.pid = QCoreApplication::applicationPid();
    info.build = runtime_build_id().toStdString();
    for (const QString& flag : runtime_debug_flags()) {
        info.debug_flags.push_back(flag.toStdString());
    }
    info.instrumentation_mode = "debug_opt_in";
    return info;
}

yodau::observability::record_header runtime_bridge::header(
    const yodau::observability::message_family family
) const {
    return yodau::observability::record_header {
        .family = family,
        .session = session_id.toStdString(),
        .monotonic_timestamp_ms = monotonic_timestamp_ms(),
    };
}

void runtime_bridge::append_sample(
    std::vector<yodau::observability::numeric_sample>& samples,
    const std::string_view metric_id, const double value
) {
    if (!std::isfinite(value)) {
        return;
    }
    samples.push_back(yodau::observability::numeric_sample {
        .metric_id = std::string(metric_id),
        .value = value,
    });
}

void runtime_bridge::append_sample(
    std::vector<yodau::observability::numeric_sample>& samples,
    const std::string_view metric_id, const qint64 value
) {
    samples.push_back(yodau::observability::numeric_sample {
        .metric_id = std::string(metric_id),
        .value = static_cast<std::int64_t>(value),
    });
}

std::vector<yodau::observability::numeric_sample>
runtime_bridge::build_samples() const {
    std::vector<yodau::observability::numeric_sample> samples;
    samples.reserve(17U);
    append_sample(
        samples, "configured_stream_count",
        static_cast<qint64>(state.inventory.configured_streams)
    );
    append_sample(
        samples, "visible_stream_count",
        static_cast<qint64>(state.inventory.visible_streams)
    );
    append_sample(
        samples, "active_stream_count",
        static_cast<qint64>(state.inventory.active_streams)
    );
    append_sample(
        samples, "configured_line_count",
        static_cast<qint64>(state.inventory.configured_lines)
    );
    append_sample(
        samples, "detected_local_source_count",
        static_cast<qint64>(state.inventory.detected_local_sources)
    );
    append_sample(
        samples, "tripwire_event_count",
        state.tripwire_event_count
    );
    append_sample(
        samples, "motion_event_count", state.motion_event_count
    );
    if (state.process_memory_rss_bytes >= 0) {
        append_sample(
            samples, "process_memory_rss_bytes",
            state.process_memory_rss_bytes
        );
    }
    append_sample(
        samples, "frame_processing_time_ms",
        state.processing.frame_processing_time_ms
    );
    append_sample(
        samples, "input_fps", state.processing.input_fps
    );
    append_sample(
        samples, "processing_fps",
        state.processing.processing_fps
    );
    append_sample(
        samples, "configured_processing_fps",
        state.processing.configured_processing_fps
    );
    append_sample(
        samples, "configured_display_fps",
        state.processing.configured_display_fps
    );
    append_sample(
        samples, "dropped_gui_frame_count",
        static_cast<qint64>(state.processing.dropped_gui_frames)
    );
    if (broadcaster != nullptr) {
        const auto transport = broadcaster->state();
        append_sample(
            samples, "monitor_queue_bytes",
            transport.queued_bytes
        );
        append_sample(
            samples, "monitor_dropped_packet_count",
            transport.dropped_low_priority_messages
                + transport.dropped_medium_priority_messages
                + transport.dropped_high_priority_messages
        );
        append_sample(
            samples, "monitor_write_error_count",
            transport.write_error_count
        );
    }
    return samples;
}

bool runtime_bridge::can_publish() const {
    return broadcaster != nullptr && broadcaster->is_enabled()
        && broadcaster->has_listener() && !goodbye_published;
}

void runtime_bridge::publish_message(
    const QByteArray& message, const publish_priority priority,
    const bool droppable
) {
    if (!can_publish()) {
        return;
    }
    using broadcaster_priority
        = yodau::monitoring::debug_broadcaster::message_priority;
    broadcaster_priority mapped = broadcaster_priority::low;
    switch (priority) {
    case publish_priority::high:
        mapped = broadcaster_priority::high;
        break;
    case publish_priority::medium:
        mapped = broadcaster_priority::medium;
        break;
    case publish_priority::low:
        mapped = broadcaster_priority::low;
        break;
    }
    broadcaster->publish_packet(message, mapped, droppable);
}

void runtime_bridge::publish_hello() {
    if (!can_publish()) {
        return;
    }
    const auto catalog = yodau::observability::metric_catalog();
    publish_message(
        yodau::data::encode_telemetry_json(
            yodau::observability::hello_record {
                .header
                = header(yodau::observability::message_family::hello),
                .identity = identity(),
                .metrics = std::vector<yodau::observability::metric_descriptor>(
                    catalog.begin(), catalog.end()
                ),
                .extensions = {},
            }
        ),
        publish_priority::high, false
    );
}

void runtime_bridge::publish_sample_batch() {
    if (!can_publish()) {
        return;
    }

    state.process_memory_rss_bytes = read_process_rss_bytes();
    publish_message(
        yodau::data::encode_telemetry_json(
            yodau::observability::sample_batch_record {
                .header = header(
                    yodau::observability::message_family::sample_batch
                ),
                .samples = build_samples(),
            }
        ),
        publish_priority::low, true
    );

    ++sample_tick_count;
    if (sample_tick_count == 1 || (sample_tick_count % 10) == 0) {
        publish_snapshot(QStringLiteral("periodic"));
    }
    emit state_changed();
}

void runtime_bridge::publish_snapshot(const QString& reason) {
    if (!can_publish()) {
        return;
    }
    state.process_memory_rss_bytes = read_process_rss_bytes();
    publish_message(
        yodau::data::encode_telemetry_json(
            yodau::observability::snapshot_record {
                .header
                = header(yodau::observability::message_family::snapshot),
                .samples = build_samples(),
                .reason = reason.toStdString(),
                .listener_connected = is_connected(),
                .endpoint_path = endpoint_path().toStdString(),
            }
        ),
        publish_priority::medium, true
    );
}

void runtime_bridge::publish_warning(
    const QString& warning_code, const QString& warning_message
) {
    if (!can_publish()) {
        return;
    }
    publish_message(
        yodau::data::encode_telemetry_json(
            yodau::observability::warning_record {
                .header
                = header(yodau::observability::message_family::warning),
                .warning_code = warning_code.toStdString(),
                .warning_message = warning_message.toStdString(),
            }
        ),
        publish_priority::high, false
    );
}

void runtime_bridge::publish_goodbye() {
    if (!can_publish()) {
        return;
    }
    publish_message(
        yodau::data::encode_telemetry_json(
            yodau::observability::goodbye_record {
                .header
                = header(yodau::observability::message_family::goodbye),
                .final_endpoint_path = endpoint_path().toStdString(),
            }
        ),
        publish_priority::high, false
    );
    goodbye_published = true;
    sample_timer->stop();
}

} // namespace yodau::monitor
