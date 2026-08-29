#ifndef YODAU_APP_MONITOR_DEBUG_PROBE_HPP
#define YODAU_APP_MONITOR_DEBUG_PROBE_HPP

#include "observability/telemetry_contract.hpp"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace yodau::monitor {

class debug_probe {
public:
    using protocol_identity = yodau::observability::producer_identity;

    static QString protocol_version_string();
    static QJsonArray protocol_message_families_v1();
    static QJsonArray protocol_required_identity_fields_v1();
    static QJsonArray required_metric_hint_fields_v1();
    static QJsonArray protocol_metric_catalog_v1();
    static QJsonObject metric_hint_for_id_v1(const QString& metric_id);
    static QJsonObject protocol_capabilities_v1();
    static QJsonObject build_protocol_message_v1(
        const QString& message_family, const protocol_identity& identity,
        qint64 monotonic_timestamp_ms, const QJsonObject& payload
    );
    static QJsonObject build_compact_protocol_message_v1(
        const QString& message_family, const QString& session_id,
        qint64 monotonic_timestamp_ms, const QJsonObject& payload
    );
    static QByteArray serialize_protocol_message_v1(const QJsonObject& message);

private:
    static QJsonArray string_list_to_json_array(const QStringList& values);
    static QJsonObject
    metric_descriptor_to_json(const yodau::observability::metric_descriptor&);
    static QJsonObject protocol_identity_to_json(const protocol_identity&);
};

} // namespace yodau::monitor

#endif // YODAU_APP_MONITOR_DEBUG_PROBE_HPP
