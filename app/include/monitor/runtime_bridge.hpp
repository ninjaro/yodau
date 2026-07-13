#ifndef YODAU_APP_MONITOR_RUNTIME_BRIDGE_HPP
#define YODAU_APP_MONITOR_RUNTIME_BRIDGE_HPP

#include "core/namespace_alias.hpp"
#include "monitor/debug_probe.hpp"
#include "streams/event.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include <chrono>
#include <vector>

class QTimer;

namespace yodau::monitoring {
class debug_broadcaster;
}

namespace yodau::monitor {

class runtime_bridge final : public QObject {
    Q_OBJECT

public:
    struct runtime_options {
        bool enabled = false;
        QString requested_endpoint;
    };

    explicit runtime_bridge(QObject* parent = nullptr);
    explicit runtime_bridge(runtime_options options, QObject* parent = nullptr);
    ~runtime_bridge() override;

    bool
    set_enabled(bool enabled, const QString& requested_endpoint = QString());
    void set_inventory(
        int configured_streams, int visible_streams, int active_streams,
        int configured_lines, int detected_local_sources
    );
    void record_event_batch(const std::vector<yodau::core::event>& events);
    void add_marker(const QString& label);

    [[nodiscard]] bool is_enabled() const;
    [[nodiscard]] QString endpoint_path() const;

signals:
    void state_changed();

private slots:
    void on_about_to_quit();
    void on_sample_timer_timeout();
    void on_broadcaster_warning(
        const QString& warning_code, const QString& warning_message
    );

private:
    struct sampled_state {
        int configured_stream_count = 0;
        int visible_stream_count = 0;
        int active_stream_count = 0;
        int configured_line_count = 0;
        int detected_local_source_count = 0;
        qint64 tripwire_event_count = 0;
        qint64 motion_event_count = 0;
        qint64 process_memory_rss_bytes = -1;
    };

    runtime_options options;
    yodau::monitoring::debug_broadcaster* broadcaster;
    QTimer* sample_timer;
    sampled_state state;
    std::chrono::steady_clock::time_point session_start_steady;
    QString session_id;
    qint64 event_sequence;
    qint64 sample_tick_count;

    static QString event_kind_to_string(yodau::core::event_kind kind);
    [[nodiscard]] qint64 monotonic_timestamp_ms() const;
    [[nodiscard]] qint64
    event_timestamp_ms(const yodau::core::event& event) const;
    [[nodiscard]] debug_probe::protocol_identity identity() const;
    static void
    append_sample(QJsonArray& samples, const QString& metric_id, qint64 value);
    [[nodiscard]] QJsonArray build_sample_array() const;
    [[nodiscard]] QJsonObject
    build_snapshot_payload(const QString& reason) const;
    void publish_hello();
    void publish_capabilities();
    void publish_sample_batch();
    void publish_snapshot(const QString& reason);
    void publish_warning(
        const QString& warning_code, const QString& warning_message
    );
    void publish_goodbye();
};

} // namespace yodau::monitor

#endif // YODAU_APP_MONITOR_RUNTIME_BRIDGE_HPP
