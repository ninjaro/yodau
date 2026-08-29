#include "monitor/debug_probe.hpp"

#include <QJsonDocument>

namespace yodau::monitor {

QJsonArray debug_probe::string_list_to_json_array(const QStringList& values) {
    QJsonArray array;
    for (const QString& value : values) {
        array.push_back(value);
    }
    return array;
}

QJsonObject debug_probe::metric_descriptor_to_json(
    const yodau::observability::metric_descriptor& metric
) {
    QJsonObject object;
    object.insert(QStringLiteral("id"), QString::fromStdString(metric.id));
    object.insert(
        QStringLiteral("label"), QString::fromStdString(metric.label)
    );
    object.insert(QStringLiteral("kind"), QString::fromStdString(metric.kind));
    object.insert(
        QStringLiteral("provenance"),
        QString::fromStdString(metric.provenance)
    );
    object.insert(QStringLiteral("unit"), QString::fromStdString(metric.unit));
    object.insert(
        QStringLiteral("scope"), QString::fromStdString(metric.scope)
    );
    object.insert(
        QStringLiteral("cardinality_semantics"),
        QString::fromStdString(metric.cardinality_semantics)
    );
    object.insert(
        QStringLiteral("stability"), QString::fromStdString(metric.stability)
    );
    object.insert(
        QStringLiteral("additive_semantics"),
        QString::fromStdString(metric.additive_semantics)
    );
    object.insert(
        QStringLiteral("confidence"), QString::fromStdString(metric.confidence)
    );
    object.insert(
        QStringLiteral("default_display_role"),
        QString::fromStdString(metric.default_display_role)
    );
    object.insert(
        QStringLiteral("domain_namespace"),
        QString::fromStdString(metric.domain_namespace)
    );
    return object;
}

QJsonObject debug_probe::protocol_identity_to_json(
    const protocol_identity& info
) {
    QJsonObject identity;
    identity.insert(QStringLiteral("app"), QString::fromStdString(info.app));
    identity.insert(QStringLiteral("pid"), static_cast<qint64>(info.pid));
    identity.insert(
        QStringLiteral("session"), QString::fromStdString(info.session)
    );
    identity.insert(
        QStringLiteral("build"), QString::fromStdString(info.build)
    );
    identity.insert(
        QStringLiteral("protocol_version"),
        QString::fromStdString(
            info.protocol.empty()
                ? std::string(yodau::observability::protocol_version)
                : info.protocol
        )
    );
    QStringList flags;
    flags.reserve(static_cast<qsizetype>(info.debug_flags.size()));
    for (const std::string& flag : info.debug_flags) {
        flags.push_back(QString::fromStdString(flag));
    }
    identity.insert(
        QStringLiteral("debug_flags"), string_list_to_json_array(flags)
    );
    identity.insert(
        QStringLiteral("instrumentation_mode"),
        QString::fromStdString(info.instrumentation_mode)
    );
    return identity;
}

QString debug_probe::protocol_version_string() {
    return QString::fromLatin1(
        yodau::observability::protocol_version.data(),
        static_cast<qsizetype>(yodau::observability::protocol_version.size())
    );
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
    static const QJsonArray metrics = [] {
        QJsonArray result;
        for (const auto& metric : yodau::observability::metric_catalog_v1()) {
            result.push_back(metric_descriptor_to_json(metric));
        }
        return result;
    }();
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
        protocol_identity_to_json(identity)
    );

    QJsonObject root = payload;
    root.insert(
        QStringLiteral("monotonic_timestamp_ms"), monotonic_timestamp_ms
    );
    root.insert(QStringLiteral("protocol_v1"), protocol);
    return root;
}

QJsonObject debug_probe::build_compact_protocol_message_v1(
    const QString& message_family, const QString& session_id,
    const qint64 monotonic_timestamp_ms, const QJsonObject& payload
) {
    QJsonObject protocol;
    protocol.insert(QStringLiteral("message_family"), message_family);
    protocol.insert(QStringLiteral("version"), protocol_version_string());
    protocol.insert(QStringLiteral("session"), session_id);

    QJsonObject root = payload;
    root.insert(
        QStringLiteral("monotonic_timestamp_ms"), monotonic_timestamp_ms
    );
    root.insert(QStringLiteral("protocol_v1"), protocol);
    return root;
}

QByteArray debug_probe::serialize_protocol_message_v1(
    const QJsonObject& message
) {
    return QJsonDocument(message).toJson(QJsonDocument::Compact);
}

} // namespace yodau::monitor
