#include "monitor/runtime_bridge.hpp"

#include "monitor/debug_broadcaster.hpp"
#include "monitor/process_memory_reader.hpp"
#include "monitor/runtime_build_info.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QUuid>

#include <QDebug>

#include <algorithm>

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
    , sample_tick_count(0) {
    sample_timer->setInterval(1000);
    sample_timer->setSingleShot(false);
    QObject::connect(
        sample_timer, &QTimer::timeout, this,
        &runtime_bridge::on_sample_timer_timeout
    );
    QObject::connect(
        broadcaster,
        &yodau::monitoring::debug_broadcaster::listener_connection_changed,
        this, &runtime_bridge::state_changed
    );
    QObject::connect(
        broadcaster, &yodau::monitoring::debug_broadcaster::warning_raised,
        this, &runtime_bridge::on_broadcaster_warning
    );
    QObject::connect(
        qApp, &QCoreApplication::aboutToQuit, this,
        &runtime_bridge::on_about_to_quit
    );

    if (options.enabled) {
        set_enabled(true, options.requested_endpoint);
    }
}

runtime_bridge::~runtime_bridge() {
    publish_snapshot(QStringLiteral("destructor"));
    publish_goodbye();
}

bool runtime_bridge::set_enabled(
    bool enabled, const QString& requested_endpoint
) {
    if (broadcaster == nullptr) {
        return false;
    }

    const bool was_enabled = broadcaster->is_enabled();
    if (was_enabled && !enabled) {
        publish_snapshot(QStringLiteral("disabled"));
        publish_goodbye();
        sample_timer->stop();
    }

    const bool changed = broadcaster->set_enabled(enabled, requested_endpoint);
    if (!enabled || !broadcaster->is_enabled()) {
        emit state_changed();
        return changed;
    }

    publish_hello();
    publish_capabilities();
    publish_snapshot(QStringLiteral("startup"));
    publish_sample_batch();
    sample_timer->start();
    qInfo() << "yodau monitor endpoint:" << endpoint_path();
    emit state_changed();
    return changed;
}

bool runtime_bridge::is_enabled() const {
    return broadcaster != nullptr && broadcaster->is_enabled();
}

QString runtime_bridge::endpoint_path() const {
    if (broadcaster == nullptr) {
        return QString();
    }
    return broadcaster->state().endpoint_name;
}

void runtime_bridge::set_inventory(
    int configured_streams, int visible_streams, int active_streams,
    int configured_lines, int detected_local_sources
) {
    const sampled_state previous = state;
    state.configured_stream_count = std::max(0, configured_streams);
    state.visible_stream_count = std::max(0, visible_streams);
    state.active_stream_count = std::max(0, active_streams);
    state.configured_line_count = std::max(0, configured_lines);
    state.detected_local_source_count = std::max(0, detected_local_sources);

    const bool changed
        = previous.configured_stream_count != state.configured_stream_count
        || previous.visible_stream_count != state.visible_stream_count
        || previous.active_stream_count != state.active_stream_count
        || previous.configured_line_count != state.configured_line_count
        || previous.detected_local_source_count
            != state.detected_local_source_count;
    if (changed && is_enabled()) {
        publish_sample_batch();
    }
}

void runtime_bridge::record_event_batch(
    const std::vector<yodau::backend::event>& events
) {
    if (events.empty()) {
        return;
    }

    QJsonArray event_array;
    for (const yodau::backend::event& event : events) {
        if (event.kind == yodau::backend::event_kind::tripwire) {
            ++state.tripwire_event_count;
        } else if (event.kind == yodau::backend::event_kind::motion) {
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
                QString::fromStdString(event.stream_name)
            );
        }
        if (!event.line_name.empty()) {
            encoded_event.insert(
                QStringLiteral("line_name"),
                QString::fromStdString(event.line_name)
            );
        }
        if (!event.message.empty()) {
            encoded_event.insert(
                QStringLiteral("message"), QString::fromStdString(event.message)
            );
        }
        if (event.pos_pct.has_value()) {
            QJsonObject pos;
            pos.insert(QStringLiteral("x"), event.pos_pct->x);
            pos.insert(QStringLiteral("y"), event.pos_pct->y);
            encoded_event.insert(QStringLiteral("position_pct"), pos);
        }
        event_array.push_back(encoded_event);
    }

    if (is_enabled()) {
        QJsonObject payload;
        payload.insert(QStringLiteral("events"), event_array);
        broadcaster->publish_json(
            debug_probe::build_protocol_message_v1(
                QStringLiteral("event_batch"), identity(),
                monotonic_timestamp_ms(), payload
            ),
            yodau::monitoring::debug_broadcaster::message_priority::medium, true
        );
        publish_sample_batch();
    }
}

void runtime_bridge::add_marker(const QString& label) {
    const QString trimmed = label.trimmed();
    if (!is_enabled() || trimmed.isEmpty()) {
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("label"), trimmed);
    broadcaster->publish_json(
        debug_probe::build_protocol_message_v1(
            QStringLiteral("marker"), identity(), monotonic_timestamp_ms(),
            payload
        ),
        yodau::monitoring::debug_broadcaster::message_priority::medium, true
    );
}

void runtime_bridge::on_about_to_quit() {
    publish_snapshot(QStringLiteral("shutdown"));
    publish_goodbye();
}

void runtime_bridge::on_sample_timer_timeout() { publish_sample_batch(); }

void runtime_bridge::on_broadcaster_warning(
    const QString& warning_code, const QString& warning_message
) {
    qWarning() << "yodau monitor warning:" << warning_code << warning_message;
    publish_warning(warning_code, warning_message);
    emit state_changed();
}

QString runtime_bridge::event_kind_to_string(yodau::backend::event_kind kind) {
    switch (kind) {
    case yodau::backend::event_kind::motion:
        return QStringLiteral("motion");
    case yodau::backend::event_kind::tripwire:
        return QStringLiteral("tripwire");
    case yodau::backend::event_kind::roi:
        return QStringLiteral("roi");
    case yodau::backend::event_kind::info:
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

qint64
runtime_bridge::event_timestamp_ms(const yodau::backend::event& event) const {
    const qint64 timestamp_ms
        = std::chrono::duration_cast<std::chrono::milliseconds>(
              event.ts - session_start_steady
        )
              .count();
    return std::max<qint64>(0, timestamp_ms);
}

debug_probe::protocol_identity runtime_bridge::identity() const {
    debug_probe::protocol_identity info;
    info.app_name = QCoreApplication::applicationName().trimmed().isEmpty()
        ? QStringLiteral("yodau")
        : QCoreApplication::applicationName().trimmed();
    info.process_id = QCoreApplication::applicationPid();
    info.session_id = session_id;
    info.build_id = runtime_build_id();
    info.protocol_version = debug_probe::protocol_version_string();
    info.debug_flags = runtime_debug_flags();
    info.instrumentation_mode = QStringLiteral("debug_opt_in");
    return info;
}

void runtime_bridge::append_sample(
    QJsonArray& samples, const QString& metric_id, qint64 value
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
        state.configured_stream_count
    );
    append_sample(
        samples, QStringLiteral("visible_stream_count"),
        state.visible_stream_count
    );
    append_sample(
        samples, QStringLiteral("active_stream_count"),
        state.active_stream_count
    );
    append_sample(
        samples, QStringLiteral("configured_line_count"),
        state.configured_line_count
    );
    append_sample(
        samples, QStringLiteral("detected_local_source_count"),
        state.detected_local_source_count
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

    return samples;
}

QJsonObject
runtime_bridge::build_snapshot_payload(const QString& reason) const {
    QJsonObject snapshot;
    snapshot.insert(
        QStringLiteral("configured_stream_count"), state.configured_stream_count
    );
    snapshot.insert(
        QStringLiteral("visible_stream_count"), state.visible_stream_count
    );
    snapshot.insert(
        QStringLiteral("active_stream_count"), state.active_stream_count
    );
    snapshot.insert(
        QStringLiteral("configured_line_count"), state.configured_line_count
    );
    snapshot.insert(
        QStringLiteral("detected_local_source_count"),
        state.detected_local_source_count
    );
    snapshot.insert(
        QStringLiteral("tripwire_event_count"), state.tripwire_event_count
    );
    snapshot.insert(
        QStringLiteral("motion_event_count"), state.motion_event_count
    );
    if (state.process_memory_rss_bytes >= 0) {
        snapshot.insert(
            QStringLiteral("process_memory_rss_bytes"),
            state.process_memory_rss_bytes
        );
    }
    snapshot.insert(QStringLiteral("reason"), reason);
    snapshot.insert(
        QStringLiteral("listener_connected"),
        broadcaster != nullptr && broadcaster->state().listener_connected
    );
    snapshot.insert(QStringLiteral("endpoint_path"), endpoint_path());

    QJsonObject payload;
    payload.insert(QStringLiteral("snapshot"), snapshot);
    return payload;
}

void runtime_bridge::publish_hello() {
    if (!is_enabled()) {
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("app"), identity().app_name);
    broadcaster->publish_json(
        debug_probe::build_protocol_message_v1(
            QStringLiteral("hello"), identity(), monotonic_timestamp_ms(),
            payload
        ),
        yodau::monitoring::debug_broadcaster::message_priority::high, false
    );
}

void runtime_bridge::publish_capabilities() {
    if (!is_enabled()) {
        return;
    }

    QJsonObject payload;
    payload.insert(
        QStringLiteral("capabilities"), debug_probe::protocol_capabilities_v1()
    );
    broadcaster->publish_json(
        debug_probe::build_protocol_message_v1(
            QStringLiteral("capabilities"), identity(),
            monotonic_timestamp_ms(), payload
        ),
        yodau::monitoring::debug_broadcaster::message_priority::high, false
    );
}

void runtime_bridge::publish_sample_batch() {
    if (!is_enabled()) {
        return;
    }

    state.process_memory_rss_bytes = read_process_rss_bytes();
    QJsonObject payload;
    payload.insert(QStringLiteral("samples"), build_sample_array());
    broadcaster->publish_json(
        debug_probe::build_protocol_message_v1(
            QStringLiteral("sample_batch"), identity(),
            monotonic_timestamp_ms(), payload
        ),
        yodau::monitoring::debug_broadcaster::message_priority::low, true
    );

    ++sample_tick_count;
    if (sample_tick_count == 1 || (sample_tick_count % 10) == 0) {
        publish_snapshot(QStringLiteral("periodic"));
    }
    emit state_changed();
}

void runtime_bridge::publish_snapshot(const QString& reason) {
    if (!is_enabled()) {
        return;
    }

    state.process_memory_rss_bytes = read_process_rss_bytes();
    broadcaster->publish_json(
        debug_probe::build_protocol_message_v1(
            QStringLiteral("snapshot"), identity(), monotonic_timestamp_ms(),
            build_snapshot_payload(reason)
        ),
        yodau::monitoring::debug_broadcaster::message_priority::medium, true
    );
}

void runtime_bridge::publish_warning(
    const QString& warning_code, const QString& warning_message
) {
    if (!is_enabled()) {
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("warning_code"), warning_code);
    payload.insert(QStringLiteral("warning_message"), warning_message);
    broadcaster->publish_json(
        debug_probe::build_protocol_message_v1(
            QStringLiteral("warning"), identity(), monotonic_timestamp_ms(),
            payload
        ),
        yodau::monitoring::debug_broadcaster::message_priority::high, false
    );
}

void runtime_bridge::publish_goodbye() {
    if (!is_enabled()) {
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("final_endpoint_path"), endpoint_path());
    broadcaster->publish_json(
        debug_probe::build_protocol_message_v1(
            QStringLiteral("goodbye"), identity(), monotonic_timestamp_ms(),
            payload
        ),
        yodau::monitoring::debug_broadcaster::message_priority::high, false
    );
}

} // namespace yodau::monitor
