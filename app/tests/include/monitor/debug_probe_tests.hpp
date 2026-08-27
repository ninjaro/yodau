#ifndef YODAU_APP_DEBUG_PROBE_TESTS_HPP
#define YODAU_APP_DEBUG_PROBE_TESTS_HPP

#include <QJsonObject>
#include <QObject>
#include <QVector>

class QLocalSocket;
class QString;

class debug_probe_tests : public QObject {
    Q_OBJECT

private slots:
    void protocol_capabilities_expose_metric_catalog();
    void protocol_message_wraps_identity_and_payload();
    void runtime_bridge_enables_local_ipc_endpoint();
    void runtime_bridge_publishes_initial_protocol_messages();
    void runtime_bridge_event_batch_updates_counts_and_payload();
    void runtime_bridge_replacement_listener_receives_fresh_bootstrap();
    void runtime_bridge_refuses_to_steal_live_endpoint();
    void runtime_bridge_endpoint_is_owner_only();

private:
    static QVector<QJsonObject> read_protocol_messages(
        QLocalSocket& socket, int minimum_messages, int timeout_ms
    );
    static QJsonObject find_message_by_family(
        const QVector<QJsonObject>& messages, const QString& family
    );
};

#endif // YODAU_APP_DEBUG_PROBE_TESTS_HPP
