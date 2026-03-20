#include "monitor/debug_broadcaster.hpp"

#include "monitor/debug_build_gate.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QRegularExpression>
#include <QUuid>

#include <algorithm>

namespace yodau::monitoring {

debug_broadcaster::debug_broadcaster(
    QObject* parent, qint64 max_queue_bytes, qint64 socket_backpressure_bytes
)
    : QObject(parent)
    , queue_byte_limit(std::max<qint64>(32 * 1024, max_queue_bytes))
    , socket_backpressure_byte_limit(
          std::max<qint64>(8 * 1024, socket_backpressure_bytes)
      )
    , runtime_enabled(false)
    , endpoint_name()
    , local_server(nullptr)
    , listener_socket(nullptr)
    , pending_packets()
    , pending_packet_bytes(0)
    , sent_messages(0)
    , dropped_low_priority_messages(0)
    , dropped_medium_priority_messages(0)
    , dropped_high_priority_messages(0)
    , write_error_count(0) { }

debug_broadcaster::~debug_broadcaster() { close_transport(); }

bool debug_broadcaster::set_enabled(
    bool enabled, const QString& requested_endpoint_name
) {
    if (!debug_monitor_compile_time_enabled()) {
        runtime_enabled = false;
        close_transport();
        if (enabled) {
            emit warning_raised(
                QStringLiteral("broadcaster_compile_time_disabled"),
                QStringLiteral("debug broadcaster unavailable in this build")
            );
        }
        return false;
    }

    if (!enabled) {
        runtime_enabled = false;
        close_transport();
        return true;
    }

    if (runtime_enabled) {
        return true;
    }

    const QString effective_endpoint = sanitize_endpoint_name(
        requested_endpoint_name.isEmpty() ? build_default_endpoint_name()
                                          : requested_endpoint_name
    );
    const QString endpoint_path = QDir::temp().filePath(
        QStringLiteral("%1.sock").arg(effective_endpoint)
    );
    if (effective_endpoint.isEmpty()) {
        emit warning_raised(
            QStringLiteral("broadcaster_invalid_endpoint"),
            QStringLiteral("unable to determine local IPC endpoint")
        );
        return false;
    }

    auto* server = new QLocalServer(this);
    QObject::connect(
        server, &QLocalServer::newConnection, this,
        &debug_broadcaster::on_new_connection
    );

    QString resolved_endpoint = endpoint_path;
    if (!endpoint_path.isEmpty()) {
        QLocalServer::removeServer(endpoint_path);
    }
    if (!effective_endpoint.isEmpty()) {
        QLocalServer::removeServer(effective_endpoint);
    }

    if (!endpoint_path.isEmpty() && !server->listen(endpoint_path)) {
        const QString path_error = server->errorString();
        if (!server->listen(effective_endpoint)) {
            emit warning_raised(
                QStringLiteral("broadcaster_listen_failed"),
                QStringLiteral("%1; fallback failed: %2")
                    .arg(path_error, server->errorString())
            );
            server->deleteLater();
            return false;
        }
        resolved_endpoint = effective_endpoint;
    }

    local_server = server;
    endpoint_name = resolved_endpoint;
    runtime_enabled = true;
    return true;
}

bool debug_broadcaster::is_enabled() const { return runtime_enabled; }

debug_broadcaster::runtime_state debug_broadcaster::state() const {
    return runtime_state {
        .compile_time_enabled = debug_monitor_compile_time_enabled(),
        .runtime_enabled = runtime_enabled,
        .listener_connected = listener_socket != nullptr
            && listener_socket->state() == QLocalSocket::ConnectedState,
        .endpoint_name = endpoint_name,
        .queued_messages = static_cast<qint64>(pending_packets.size()),
        .queued_bytes = pending_packet_bytes,
        .sent_messages = sent_messages,
        .dropped_low_priority_messages = dropped_low_priority_messages,
        .dropped_medium_priority_messages = dropped_medium_priority_messages,
        .dropped_high_priority_messages = dropped_high_priority_messages,
        .write_error_count = write_error_count,
    };
}

bool debug_broadcaster::publish_json(
    const QJsonObject& message, message_priority priority, bool droppable
) {
    if (!runtime_enabled) {
        return false;
    }

    QJsonDocument document(message);
    QByteArray payload = document.toJson(QJsonDocument::Compact);
    payload.append('\n');

    const qint64 payload_bytes = static_cast<qint64>(payload.size());
    if (!make_room_for_packet(payload_bytes, priority, droppable)) {
        mark_dropped(priority);
        return false;
    }

    enqueue_packet(
        queued_packet {
            .payload = std::move(payload),
            .priority = priority,
            .droppable = droppable,
        }
    );
    try_flush();
    return true;
}

void debug_broadcaster::on_new_connection() {
    if (local_server == nullptr) {
        return;
    }

    QLocalSocket* newest_socket = nullptr;
    while (local_server->hasPendingConnections()) {
        newest_socket = local_server->nextPendingConnection();
    }
    if (newest_socket == nullptr) {
        return;
    }

    if (listener_socket != nullptr) {
        listener_socket->disconnect(this);
        listener_socket->close();
        listener_socket->deleteLater();
        listener_socket = nullptr;
    }

    attach_socket(newest_socket);
    emit listener_connection_changed(true);
    try_flush();
}

void debug_broadcaster::on_socket_disconnected() {
    clear_socket();
    emit listener_connection_changed(false);
}

void debug_broadcaster::on_socket_bytes_written(qint64 bytes_written) {
    Q_UNUSED(bytes_written);
    try_flush();
}

QString debug_broadcaster::build_default_endpoint_name() const {
    const QString suffix
        = QUuid::createUuid().toString(QUuid::WithoutBraces).left(6);
    return QStringLiteral("yodau_%1_%2")
        .arg(QCoreApplication::applicationPid())
        .arg(suffix);
}

QString debug_broadcaster::sanitize_endpoint_name(const QString& raw_name) {
    QString endpoint = raw_name.trimmed();
    endpoint.replace(
        QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]+")),
        QStringLiteral("_")
    );

    constexpr int max_endpoint_len = 24;
    if (endpoint.size() > max_endpoint_len) {
        endpoint = endpoint.left(12) + QLatin1Char('_') + endpoint.right(11);
    }
    return endpoint;
}

void debug_broadcaster::close_transport() {
    clear_socket();

    if (local_server != nullptr) {
        const QString existing_endpoint = endpoint_name;
        local_server->close();
        local_server->deleteLater();
        local_server = nullptr;
        if (!existing_endpoint.isEmpty()) {
            QLocalServer::removeServer(existing_endpoint);
        }
    }

    pending_packets.clear();
    pending_packet_bytes = 0;
    endpoint_name.clear();
}

void debug_broadcaster::mark_dropped(message_priority priority) {
    switch (priority) {
    case message_priority::high:
        ++dropped_high_priority_messages;
        return;
    case message_priority::medium:
        ++dropped_medium_priority_messages;
        return;
    case message_priority::low:
    default:
        ++dropped_low_priority_messages;
        return;
    }
}

void debug_broadcaster::enqueue_packet(queued_packet&& packet) {
    pending_packet_bytes += static_cast<qint64>(packet.payload.size());
    pending_packets.push_back(std::move(packet));
}

bool debug_broadcaster::drop_oldest_packet_of_priority(
    message_priority priority
) {
    for (int index = 0; index < pending_packets.size(); ++index) {
        const queued_packet& packet = pending_packets.at(index);
        if (!packet.droppable || packet.priority != priority) {
            continue;
        }

        pending_packet_bytes -= static_cast<qint64>(packet.payload.size());
        pending_packets.removeAt(index);
        mark_dropped(priority);
        return true;
    }

    return false;
}

bool debug_broadcaster::make_room_for_packet(
    qint64 packet_bytes, message_priority incoming_priority,
    bool incoming_droppable
) {
    if (packet_bytes <= 0) {
        return false;
    }
    if (packet_bytes > queue_byte_limit && incoming_droppable) {
        return false;
    }
    if (pending_packet_bytes + packet_bytes <= queue_byte_limit) {
        return true;
    }

    QVector<message_priority> drop_order;
    switch (incoming_priority) {
    case message_priority::high:
        drop_order = {
            message_priority::low,
            message_priority::medium,
            message_priority::high,
        };
        break;
    case message_priority::medium:
        drop_order = {
            message_priority::low,
            message_priority::medium,
        };
        break;
    case message_priority::low:
    default:
        drop_order = {
            message_priority::low,
        };
        break;
    }

    while (pending_packet_bytes + packet_bytes > queue_byte_limit) {
        bool dropped = false;
        for (message_priority priority : drop_order) {
            if (drop_oldest_packet_of_priority(priority)) {
                dropped = true;
                break;
            }
        }
        if (!dropped) {
            return false;
        }
    }

    return true;
}

void debug_broadcaster::try_flush() {
    if (listener_socket == nullptr
        || listener_socket->state() != QLocalSocket::ConnectedState) {
        return;
    }

    while (!pending_packets.isEmpty()) {
        if (listener_socket->bytesToWrite() > socket_backpressure_byte_limit) {
            return;
        }

        const queued_packet packet = pending_packets.takeFirst();
        pending_packet_bytes -= static_cast<qint64>(packet.payload.size());
        const qint64 written = listener_socket->write(packet.payload);
        if (written < 0) {
            ++write_error_count;
            clear_socket();
            emit listener_connection_changed(false);
            return;
        }

        ++sent_messages;
    }
}

void debug_broadcaster::attach_socket(QLocalSocket* socket) {
    if (socket == nullptr) {
        return;
    }

    listener_socket = socket;
    QObject::connect(
        listener_socket, &QLocalSocket::disconnected, this,
        &debug_broadcaster::on_socket_disconnected
    );
    QObject::connect(
        listener_socket, &QLocalSocket::bytesWritten, this,
        &debug_broadcaster::on_socket_bytes_written
    );
}

void debug_broadcaster::clear_socket() {
    if (listener_socket == nullptr) {
        return;
    }

    listener_socket->disconnect(this);
    listener_socket->close();
    listener_socket->deleteLater();
    listener_socket = nullptr;
}

} // namespace yodau::monitoring
