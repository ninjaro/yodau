#ifndef YODAU_APP_MONITOR_DEBUG_PROBE_HPP
#define YODAU_APP_MONITOR_DEBUG_PROBE_HPP

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace yodau::monitor {

class debug_probe {
public:
    struct protocol_identity {
        QString app_name;
        qint64 process_id = -1;
        QString session_id;
        QString build_id;
        QString protocol_version;
        QStringList debug_flags;
        QString instrumentation_mode;
    };

    static QString protocol_version_string();
    static QJsonArray protocol_message_families_v1();
    static QJsonArray protocol_required_identity_fields_v1();
    static QJsonArray protocol_required_metric_hint_fields_v1();
    static QJsonArray protocol_metric_catalog_v1();
    static QJsonObject protocol_metric_hint_for_id_v1(const QString& metric_id);
    static QJsonObject protocol_capabilities_v1();
    static QJsonObject build_protocol_message_v1(
        const QString& message_family, const protocol_identity& identity,
        qint64 monotonic_timestamp_ms, const QJsonObject& payload
    );

private:
    static QJsonArray string_list_to_json_array(const QStringList& values);
    static QJsonObject metric_catalog_entry(
        const QString& id, const QString& label, const QString& kind,
        const QString& provenance, const QString& unit, const QString& scope,
        const QString& cardinality_semantics, const QString& stability,
        const QString& additive_semantics, const QString& confidence,
        const QString& default_display_role, const QString& domain_namespace
    );
    static QJsonObject protocol_identity_to_json(
        const QString& app_name, qint64 process_id, const QString& session_id,
        const QString& build_id, const QString& protocol_version,
        const QStringList& debug_flags, const QString& instrumentation_mode
    );
};

} // namespace yodau::monitor

#endif // YODAU_APP_MONITOR_DEBUG_PROBE_HPP
