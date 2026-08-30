#ifndef YODAU_APP_MONITOR_RUNTIME_BRIDGE_HPP
#define YODAU_APP_MONITOR_RUNTIME_BRIDGE_HPP

#include "debug/runtime_observer.hpp"
#include "debug/telemetry_records.hpp"

#include <QByteArray>
#include <QObject>
#include <QString>

#include <chrono>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class QTimer;

namespace yodau::monitoring {
class debug_broadcaster;
}

namespace yodau::monitor {

// Qt-thread adapter for the portable runtime_observer contract. Update methods
// only replace bounded scalar state. JSON construction and socket publication
// happen at the one-second cadence and only while a listener is connected.
class runtime_bridge final
    : public QObject
    , public yodau::observability::runtime_observer {
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

    void update_inventory(
        const yodau::observability::inventory_statistics& value
    ) noexcept override;
    void update_processing(
        const yodau::observability::processing_statistics& value
    ) noexcept override;
    [[nodiscard]] bool wants_event_details() const noexcept override;
    void record_events(
        std::span<const yodau::observability::runtime_event_view> events
    ) override;
    void add_marker(std::string_view label) override;

    // Convenience for the small UI inventory call sites.
    void set_inventory(
        int configured_streams, int visible_streams, int active_streams,
        int configured_lines, int detected_local_sources
    ) noexcept;

    [[nodiscard]] bool is_enabled() const;
    [[nodiscard]] bool is_connected() const;
    [[nodiscard]] QString endpoint_path() const;
    [[nodiscard]] qint64 queued_bytes() const;
    [[nodiscard]] qint64 published_message_count() const;

signals:
    void state_changed();

private slots:
    void on_about_to_quit();
    void on_sample_timer_timeout();
    void on_broadcaster_warning(
        const QString& warning_code, const QString& warning_message
    );

private:
    enum class publish_priority {
        high,
        medium,
        low,
    };

    struct sampled_state {
        yodau::observability::inventory_statistics inventory;
        yodau::observability::processing_statistics processing;
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
    bool goodbye_published;

    [[nodiscard]] qint64 monotonic_timestamp_ms() const;
    [[nodiscard]] qint64 event_timestamp_ms(
        const yodau::observability::runtime_event_view& event
    ) const;
    [[nodiscard]] yodau::observability::producer_identity identity() const;
    [[nodiscard]] yodau::observability::record_header header(
        yodau::observability::message_family family
    ) const;
    static void append_sample(
        std::vector<yodau::observability::numeric_sample>& samples,
        std::string_view metric_id, double value
    );
    static void append_sample(
        std::vector<yodau::observability::numeric_sample>& samples,
        std::string_view metric_id, qint64 value
    );
    [[nodiscard]] std::vector<yodau::observability::numeric_sample>
    build_samples() const;
    [[nodiscard]] bool can_publish() const;
    void publish_message(
        const QByteArray& message, publish_priority priority, bool droppable
    );
    void publish_hello();
    void publish_sample_batch();
    void publish_snapshot(const QString& reason);
    void publish_warning(
        const QString& warning_code, const QString& warning_message
    );
    void publish_goodbye();
};

} // namespace yodau::monitor

#endif // YODAU_APP_MONITOR_RUNTIME_BRIDGE_HPP
