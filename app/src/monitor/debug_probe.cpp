#include "monitor/debug_probe.hpp"

namespace yodau::monitor {

QJsonArray debug_probe::string_list_to_json_array(const QStringList& values) {
    QJsonArray array;
    for (const QString& value : values) {
        array.push_back(value);
    }
    return array;
}

QJsonObject debug_probe::metric_catalog_entry(
    const QString& id, const QString& label, const QString& kind,
    const QString& provenance, const QString& unit, const QString& scope,
    const QString& cardinality_semantics, const QString& stability,
    const QString& additive_semantics, const QString& confidence,
    const QString& default_display_role, const QString& domain_namespace
) {
    QJsonObject object;
    object.insert(QStringLiteral("id"), id);
    object.insert(QStringLiteral("label"), label);
    object.insert(QStringLiteral("kind"), kind);
    object.insert(QStringLiteral("provenance"), provenance);
    object.insert(QStringLiteral("unit"), unit);
    object.insert(QStringLiteral("scope"), scope);
    object.insert(
        QStringLiteral("cardinality_semantics"), cardinality_semantics
    );
    object.insert(QStringLiteral("stability"), stability);
    object.insert(QStringLiteral("additive_semantics"), additive_semantics);
    object.insert(QStringLiteral("confidence"), confidence);
    object.insert(QStringLiteral("default_display_role"), default_display_role);
    object.insert(QStringLiteral("domain_namespace"), domain_namespace);
    return object;
}

QJsonObject debug_probe::protocol_identity_to_json(
    const QString& app_name, qint64 process_id, const QString& session_id,
    const QString& build_id, const QString& protocol_version,
    const QStringList& debug_flags, const QString& instrumentation_mode
) {
    QJsonObject identity;
    identity.insert(QStringLiteral("app"), app_name);
    identity.insert(QStringLiteral("pid"), process_id);
    identity.insert(QStringLiteral("session"), session_id);
    identity.insert(QStringLiteral("build"), build_id);
    identity.insert(QStringLiteral("protocol_version"), protocol_version);
    identity.insert(
        QStringLiteral("debug_flags"), string_list_to_json_array(debug_flags)
    );
    identity.insert(
        QStringLiteral("instrumentation_mode"), instrumentation_mode
    );
    return identity;
}

QString debug_probe::protocol_version_string() {
    return QStringLiteral("debug_telemetry.v1");
}

QJsonArray debug_probe::protocol_message_families_v1() {
    return string_list_to_json_array(
        {
            QStringLiteral("hello"),
            QStringLiteral("capabilities"),
            QStringLiteral("sample_batch"),
            QStringLiteral("event_batch"),
            QStringLiteral("snapshot"),
            QStringLiteral("marker"),
            QStringLiteral("warning"),
            QStringLiteral("goodbye"),
        }
    );
}

QJsonArray debug_probe::protocol_required_identity_fields_v1() {
    return string_list_to_json_array(
        {
            QStringLiteral("app"),
            QStringLiteral("pid"),
            QStringLiteral("session"),
            QStringLiteral("build"),
            QStringLiteral("protocol_version"),
            QStringLiteral("debug_flags"),
            QStringLiteral("instrumentation_mode"),
        }
    );
}

QJsonArray debug_probe::required_metric_hint_fields_v1() {
    return string_list_to_json_array(
        {
            QStringLiteral("kind"),
            QStringLiteral("provenance"),
            QStringLiteral("unit"),
            QStringLiteral("scope"),
            QStringLiteral("cardinality_semantics"),
            QStringLiteral("stability"),
            QStringLiteral("additive_semantics"),
            QStringLiteral("confidence"),
            QStringLiteral("default_display_role"),
            QStringLiteral("domain_namespace"),
        }
    );
}

QJsonArray debug_probe::protocol_metric_catalog_v1() {
    QJsonArray metrics;
    metrics.push_back(metric_catalog_entry(
        QStringLiteral("process_memory_rss_bytes"),
        QStringLiteral("Process RSS (measured)"), QStringLiteral("memory"),
        QStringLiteral("measured"), QStringLiteral("bytes"),
        QStringLiteral("process"), QStringLiteral("stock"),
        QStringLiteral("stable"), QStringLiteral("non_additive_system_total"),
        QStringLiteral("exact"), QStringLiteral("primary"),
        QStringLiteral("system.process")
    ));
    metrics.push_back(metric_catalog_entry(
        QStringLiteral("configured_stream_count"),
        QStringLiteral("Configured streams"), QStringLiteral("count"),
        QStringLiteral("accounted"), QStringLiteral("count"),
        QStringLiteral("application"), QStringLiteral("stock"),
        QStringLiteral("stable"), QStringLiteral("additive_within_scope"),
        QStringLiteral("exact"), QStringLiteral("secondary"),
        QStringLiteral("yodau.streams")
    ));
    metrics.push_back(metric_catalog_entry(
        QStringLiteral("visible_stream_count"),
        QStringLiteral("Visible streams"), QStringLiteral("count"),
        QStringLiteral("accounted"), QStringLiteral("count"),
        QStringLiteral("application"), QStringLiteral("stock"),
        QStringLiteral("recent_window"),
        QStringLiteral("additive_within_scope"), QStringLiteral("exact"),
        QStringLiteral("secondary"), QStringLiteral("yodau.streams")
    ));
    metrics.push_back(metric_catalog_entry(
        QStringLiteral("active_stream_count"), QStringLiteral("Active streams"),
        QStringLiteral("count"), QStringLiteral("accounted"),
        QStringLiteral("count"), QStringLiteral("application"),
        QStringLiteral("state"), QStringLiteral("stable"),
        QStringLiteral("non_additive"), QStringLiteral("exact"),
        QStringLiteral("secondary"), QStringLiteral("yodau.streams")
    ));
    metrics.push_back(metric_catalog_entry(
        QStringLiteral("configured_line_count"),
        QStringLiteral("Configured lines"), QStringLiteral("count"),
        QStringLiteral("accounted"), QStringLiteral("count"),
        QStringLiteral("application"), QStringLiteral("stock"),
        QStringLiteral("stable"), QStringLiteral("additive_within_scope"),
        QStringLiteral("exact"), QStringLiteral("secondary"),
        QStringLiteral("yodau.streams")
    ));
    metrics.push_back(metric_catalog_entry(
        QStringLiteral("detected_local_source_count"),
        QStringLiteral("Detected local sources"), QStringLiteral("count"),
        QStringLiteral("measured"), QStringLiteral("count"),
        QStringLiteral("application"), QStringLiteral("stock"),
        QStringLiteral("recent_window"),
        QStringLiteral("additive_within_scope"), QStringLiteral("exact"),
        QStringLiteral("secondary"), QStringLiteral("yodau.streams")
    ));
    metrics.push_back(metric_catalog_entry(
        QStringLiteral("tripwire_event_count"),
        QStringLiteral("Tripwire events"), QStringLiteral("count"),
        QStringLiteral("measured"), QStringLiteral("count"),
        QStringLiteral("session"), QStringLiteral("stock"),
        QStringLiteral("monotonic"), QStringLiteral("additive_within_scope"),
        QStringLiteral("exact"), QStringLiteral("secondary"),
        QStringLiteral("yodau.analysis")
    ));
    metrics.push_back(metric_catalog_entry(
        QStringLiteral("motion_event_count"), QStringLiteral("Motion events"),
        QStringLiteral("count"), QStringLiteral("measured"),
        QStringLiteral("count"), QStringLiteral("session"),
        QStringLiteral("stock"), QStringLiteral("monotonic"),
        QStringLiteral("additive_within_scope"), QStringLiteral("exact"),
        QStringLiteral("secondary"), QStringLiteral("yodau.analysis")
    ));
    return metrics;
}

QJsonObject debug_probe::metric_hint_for_id_v1(const QString& metric_id) {
    if (metric_id.isEmpty()) {
        return {};
    }

    const QJsonArray catalog = protocol_metric_catalog_v1();
    for (const auto& value : catalog) {
        const QJsonObject metric = value.toObject();
        if (metric.value(QStringLiteral("id")).toString() == metric_id) {
            return metric;
        }
    }
    return {};
}

QJsonObject debug_probe::protocol_capabilities_v1() {
    QJsonObject capabilities;
    capabilities.insert(
        QStringLiteral("domains"),
        string_list_to_json_array(
            {
                QStringLiteral("system.process"),
                QStringLiteral("yodau.streams"),
                QStringLiteral("yodau.analysis"),
            }
        )
    );
    capabilities.insert(
        QStringLiteral("optional_streams"),
        string_list_to_json_array(
            {
                QStringLiteral("sample_batch"),
                QStringLiteral("event_batch"),
                QStringLiteral("snapshot"),
                QStringLiteral("marker"),
                QStringLiteral("warning"),
            }
        )
    );
    capabilities.insert(
        QStringLiteral("metric_catalog"), protocol_metric_catalog_v1()
    );
    capabilities.insert(
        QStringLiteral("required_metric_hint_fields"),
        required_metric_hint_fields_v1()
    );
    capabilities.insert(
        QStringLiteral("card_image_domain_hint_fields"), QJsonArray()
    );
    return capabilities;
}

QJsonObject debug_probe::build_protocol_message_v1(
    const QString& message_family, const protocol_identity& identity,
    qint64 monotonic_timestamp_ms, const QJsonObject& payload
) {
    QJsonObject protocol;
    protocol.insert(QStringLiteral("message_family"), message_family);
    protocol.insert(QStringLiteral("version"), protocol_version_string());
    protocol.insert(
        QStringLiteral("message_families"), protocol_message_families_v1()
    );
    protocol.insert(
        QStringLiteral("required_identity_fields"),
        protocol_required_identity_fields_v1()
    );
    protocol.insert(
        QStringLiteral("required_metric_hint_fields"),
        required_metric_hint_fields_v1()
    );
    protocol.insert(QStringLiteral("capabilities"), protocol_capabilities_v1());
    protocol.insert(
        QStringLiteral("identity"),
        protocol_identity_to_json(
            identity.app_name, identity.process_id, identity.session_id,
            identity.build_id,
            identity.protocol_version.isEmpty() ? protocol_version_string()
                                                : identity.protocol_version,
            identity.debug_flags, identity.instrumentation_mode
        )
    );

    QJsonObject root = payload;
    root.insert(
        QStringLiteral("monotonic_timestamp_ms"), monotonic_timestamp_ms
    );
    root.insert(QStringLiteral("protocol_v1"), protocol);
    return root;
}

} // namespace yodau::monitor
