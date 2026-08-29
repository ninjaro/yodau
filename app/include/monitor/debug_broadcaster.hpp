#ifndef YODAU_APP_MONITOR_DEBUG_BROADCASTER_HPP
#define YODAU_APP_MONITOR_DEBUG_BROADCASTER_HPP

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVector>

#include <memory>

class QLockFile;
class QLocalServer;
class QLocalSocket;

namespace yodau::monitoring {

class debug_broadcaster : public QObject {
    Q_OBJECT

public:
    enum class message_priority {
        high,
        medium,
        low,
    };

    struct runtime_state {
        bool compile_time_enabled = false;
        bool runtime_enabled = false;
        bool listener_connected = false;
        QString endpoint_name;
        qint64 queued_messages = 0;
        qint64 queued_bytes = 0;
        qint64 sent_messages = 0;
        qint64 published_messages = 0;
        qint64 published_bytes = 0;
        qint64 dropped_low_priority_messages = 0;
        qint64 dropped_medium_priority_messages = 0;
        qint64 dropped_high_priority_messages = 0;
        qint64 write_error_count = 0;
    };

    explicit debug_broadcaster(
        QObject* parent = nullptr, qint64 max_queue_bytes = 512 * 1024,
        qint64 socket_backpressure_bytes = 128 * 1024
    );
    ~debug_broadcaster() override;

    bool set_enabled(
        bool enabled, const QString& requested_endpoint_name = QString()
    );
    [[nodiscard]] bool is_enabled() const;
    [[nodiscard]] bool has_listener() const;
    [[nodiscard]] runtime_state state() const;

    bool publish_packet(
        QByteArray payload, message_priority priority, bool droppable
    );

signals:
    void listener_connection_changed(bool connected);
    void warning_raised(const QString& warning_code, const QString& message);

private slots:
    void on_new_connection();
    void on_socket_disconnected();
    void on_socket_bytes_written(qint64 bytes_written);

private:
    struct queued_packet {
        QByteArray payload;
        message_priority priority = message_priority::low;
        bool droppable = true;
    };

    [[nodiscard]] static QString build_default_endpoint_name();
    static QString sanitize_endpoint_name(const QString& raw_name);
    static QString resolve_endpoint_path(const QString& requested_name);
    void close_transport();
    void mark_dropped(message_priority priority);
    void enqueue_packet(queued_packet&& packet);
    bool drop_oldest_packet_of_priority(message_priority priority);
    bool make_room_for_packet(
        qint64 packet_bytes, message_priority incoming_priority,
        bool incoming_droppable
    );
    void try_flush();
    void attach_socket(QLocalSocket* socket);
    void clear_socket();

    qint64 queue_byte_limit;
    qint64 socket_backpressure_byte_limit;
    bool runtime_enabled;
    QString endpoint_name;
    std::unique_ptr<QLockFile> endpoint_lock;
    QLocalServer* local_server;
    QLocalSocket* listener_socket;
    QVector<queued_packet> pending_packets;
    qint64 pending_packet_bytes;
    qint64 sent_messages;
    qint64 published_messages;
    qint64 published_bytes;
    qint64 dropped_low_priority_messages;
    qint64 dropped_medium_priority_messages;
    qint64 dropped_high_priority_messages;
    qint64 write_error_count;
};

} // namespace yodau::monitoring

#endif // YODAU_APP_MONITOR_DEBUG_BROADCASTER_HPP
