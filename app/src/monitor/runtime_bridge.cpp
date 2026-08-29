#include "monitor/runtime_bridge.hpp"

#include "monitor/debug_broadcaster.hpp"
#include "monitor/debug_probe.hpp"
#include "monitor/process_memory_reader.hpp"
#include "monitor/runtime_build_info.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QUuid>

#include <QDebug>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace yodau::monitor {

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
                publish_capabilities();
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
    state.processing.dropped_gui_frames = value.dropped_gui_frames;
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

    QJsonArray event_array;
    for (const auto& event : events) {
        if (event.kind == yodau::observability::runtime_event_kind::tripwire) {
            ++state.tripwire_event_count;
        } else if (
            event.kind == yodau::observability::runtime_event_kind::motion
        ) {
            ++state.motion_event_count;
        }

        QJsonObject encoded_event;
        encoded_event.insert(
            QStringLiteral("collector_sequence"), ++event_sequence
        );
        encoded_event.insert(
            QStringLiteral("kind"), event_kind_to_string(event.kind)
        );
        encoded_event.insert(
            QStringLiteral("timestamp_ms"), event_timestamp_ms(event)
        );
        if (!event.stream_name.empty()) {
            encoded_event.insert(
                QStringLiteral("stream_name"),
                QString::fromUtf8(
                    event.stream_name.data(),
                    static_cast<qsizetype>(event.stream_name.size())
                )
            );
        }
        if (!event.line_name.empty()) {
            encoded_event.insert(
                QStringLiteral("line_name"),
                QString::fromUtf8(
                    event.line_name.data(),
                    static_cast<qsizetype>(event.line_name.size())
                )
            );
        }
        if (!event.message.empty()) {
            encoded_event.insert(
                QStringLiteral("message"),
                QString::fromUtf8(
                    event.message.data(),
                    static_cast<qsizetype>(event.message.size())
                )
            );
        }
        if (event.position_x_pct.has_value()
            && event.position_y_pct.has_value()) {
            QJsonObject position;
            position.insert(QStringLiteral("x"), *event.position_x_pct);
            position.insert(QStringLiteral("y"), *event.position_y_pct);
            encoded_event.insert(QStringLiteral("position_pct"), position);
        }
        event_array.push_back(encoded_event);
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("event_count"), event_array.size());
    payload.insert(QStringLiteral("events"), event_array);
    publish_message(
        debug_probe::build_compact_protocol_message_v1(
            QStringLiteral("event_batch"), session_id,
            monotonic_timestamp_ms(), payload
        ),
        publish_priority::medium, true
    );
}

void runtime_bridge::add_marker(const std::string_view label) {
    if (label.empty() || !can_publish()) {
        return;
    }
    const QString trimmed
        = QString::fromUtf8(
              label.data(), static_cast<qsizetype>(label.size())
          )
              .trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("label"), trimmed);
    publish_message(
        debug_probe::build_compact_protocol_message_v1(
            QStringLiteral("marker"), session_id, monotonic_timestamp_ms(),
            payload
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

QString runtime_bridge::event_kind_to_string(
    const yodau::observability::runtime_event_kind kind
) {
    switch (kind) {
    case yodau::observability::runtime_event_kind::motion:
        return QStringLiteral("motion");
    case yodau::observability::runtime_event_kind::tripwire:
        return QStringLiteral("tripwire");
    case yodau::observability::runtime_event_kind::roi:
        return QStringLiteral("roi");
    case yodau::observability::runtime_event_kind::info:
    default:
        return QStringLiteral("info");
    }
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
    info.session = session_id.toStdString();
    info.build = runtime_build_id().toStdString();
    info.protocol = std::string(yodau::observability::protocol_version);
    for (const QString& flag : runtime_debug_flags()) {
        info.debug_flags.push_back(flag.toStdString());
    }
    info.instrumentation_mode = "debug_opt_in";
    return info;
}

void runtime_bridge::append_sample(
    QJsonArray& samples, const QString& metric_id, const double value
) {
    if (!std::isfinite(value)) {
        return;
    }
    QJsonObject sample;
    sample.insert(QStringLiteral("metric_id"), metric_id);
    sample.insert(QStringLiteral("value"), value);
    samples.push_back(sample);
}

void runtime_bridge::append_sample(
    QJsonArray& samples, const QString& metric_id, const qint64 value
) {
    QJsonObject sample;
    sample.insert(QStringLiteral("metric_id"), metric_id);
    sample.insert(QStringLiteral("value"), value);
    samples.push_back(sample);
}

QJsonArray runtime_bridge::build_sample_array() const {
    QJsonArray samples;
    append_sample(
        samples, QStringLiteral("configured_stream_count"),
        static_cast<qint64>(state.inventory.configured_streams)
    );
    append_sample(
        samples, QStringLiteral("visible_stream_count"),
        static_cast<qint64>(state.inventory.visible_streams)
    );
    append_sample(
        samples, QStringLiteral("active_stream_count"),
        static_cast<qint64>(state.inventory.active_streams)
    );
    append_sample(
        samples, QStringLiteral("configured_line_count"),
        static_cast<qint64>(state.inventory.configured_lines)
    );
    append_sample(
        samples, QStringLiteral("detected_local_source_count"),
        static_cast<qint64>(state.inventory.detected_local_sources)
    );
    append_sample(
        samples, QStringLiteral("tripwire_event_count"),
        state.tripwire_event_count
    );
    append_sample(
        samples, QStringLiteral("motion_event_count"), state.motion_event_count
    );
    if (state.process_memory_rss_bytes >= 0) {
        append_sample(
            samples, QStringLiteral("process_memory_rss_bytes"),
            state.process_memory_rss_bytes
        );
    }
    append_sample(
        samples, QStringLiteral("frame_processing_time_ms"),
        state.processing.frame_processing_time_ms
    );
    append_sample(
        samples, QStringLiteral("input_fps"), state.processing.input_fps
    );
    append_sample(
        samples, QStringLiteral("processing_fps"),
        state.processing.processing_fps
    );
    append_sample(
        samples, QStringLiteral("configured_processing_fps"),
        state.processing.configured_processing_fps
    );
    append_sample(
        samples, QStringLiteral("configured_display_fps"),
        state.processing.configured_display_fps
    );
    append_sample(
        samples, QStringLiteral("dropped_gui_frame_count"),
        static_cast<double>(state.processing.dropped_gui_frames)
    );
    if (broadcaster != nullptr) {
        const auto transport = broadcaster->state();
        append_sample(
            samples, QStringLiteral("monitor_queue_bytes"),
            transport.queued_bytes
        );
        append_sample(
            samples, QStringLiteral("monitor_dropped_packet_count"),
            transport.dropped_low_priority_messages
                + transport.dropped_medium_priority_messages
                + transport.dropped_high_priority_messages
        );
        append_sample(
            samples, QStringLiteral("monitor_write_error_count"),
            transport.write_error_count
        );
    }
    return samples;
}

QJsonObject runtime_bridge::build_snapshot_payload(
    const QString& reason
) const {
    QJsonObject snapshot;
    for (const auto& value : build_sample_array()) {
        const QJsonObject sample = value.toObject();
        snapshot.insert(
            sample.value(QStringLiteral("metric_id")).toString(),
            sample.value(QStringLiteral("value"))
        );
    }
    snapshot.insert(QStringLiteral("reason"), reason);
    snapshot.insert(QStringLiteral("listener_connected"), is_connected());
    snapshot.insert(QStringLiteral("endpoint_path"), endpoint_path());

    QJsonObject payload;
    payload.insert(QStringLiteral("snapshot"), snapshot);
    return payload;
}

bool runtime_bridge::can_publish() const {
    return broadcaster != nullptr && broadcaster->is_enabled()
        && broadcaster->has_listener() && !goodbye_published;
}

void runtime_bridge::publish_message(
    const QJsonObject& message, const publish_priority priority,
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
    broadcaster->publish_packet(
        debug_probe::serialize_protocol_message_v1(message), mapped, droppable
    );
}

void runtime_bridge::publish_hello() {
    if (!can_publish()) {
        return;
    }
    const auto info = identity();
    QJsonObject payload;
    payload.insert(QStringLiteral("app"), QString::fromStdString(info.app));
    publish_message(
        debug_probe::build_protocol_message_v1(
            QStringLiteral("hello"), info, monotonic_timestamp_ms(), payload
        ),
        publish_priority::high, false
    );
}

void runtime_bridge::publish_capabilities() {
    if (!can_publish()) {
        return;
    }
    QJsonObject payload;
    payload.insert(
        QStringLiteral("capabilities"), debug_probe::protocol_capabilities_v1()
    );
    publish_message(
        debug_probe::build_protocol_message_v1(
            QStringLiteral("capabilities"), identity(),
            monotonic_timestamp_ms(), payload
        ),
        publish_priority::high, false
    );
}

void runtime_bridge::publish_sample_batch() {
    if (!can_publish()) {
        return;
    }

    state.process_memory_rss_bytes = read_process_rss_bytes();
    const QJsonArray samples = build_sample_array();
    QJsonObject payload;
    payload.insert(QStringLiteral("sample_count"), samples.size());
    payload.insert(QStringLiteral("samples"), samples);
    publish_message(
        debug_probe::build_compact_protocol_message_v1(
            QStringLiteral("sample_batch"), session_id,
            monotonic_timestamp_ms(), payload
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
        debug_probe::build_compact_protocol_message_v1(
            QStringLiteral("snapshot"), session_id, monotonic_timestamp_ms(),
            build_snapshot_payload(reason)
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
    QJsonObject payload;
    payload.insert(QStringLiteral("warning_code"), warning_code);
    payload.insert(QStringLiteral("warning_message"), warning_message);
    publish_message(
        debug_probe::build_compact_protocol_message_v1(
            QStringLiteral("warning"), session_id, monotonic_timestamp_ms(),
            payload
        ),
        publish_priority::high, false
    );
}

void runtime_bridge::publish_goodbye() {
    if (!can_publish()) {
        return;
    }
    QJsonObject payload;
    payload.insert(QStringLiteral("final_endpoint_path"), endpoint_path());
    publish_message(
        debug_probe::build_compact_protocol_message_v1(
            QStringLiteral("goodbye"), session_id, monotonic_timestamp_ms(),
            payload
        ),
        publish_priority::high, false
    );
    goodbye_published = true;
    sample_timer->stop();
}

} // namespace yodau::monitor
