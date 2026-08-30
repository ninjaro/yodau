#include "monitor/telemetry_json.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <type_traits>

namespace yodau::data {
namespace {

using namespace yodau::observability;

QJsonObject header_json(const record_header& header) {
    return QJsonObject {
        { QStringLiteral("family"),
          QString::fromUtf8(family_token(header.family)) },
        { QStringLiteral("session"), QString::fromStdString(header.session) },
        { QStringLiteral("monotonic_timestamp_ms"),
          static_cast<qint64>(header.monotonic_timestamp_ms) },
    };
}

QJsonObject identity_json(const producer_identity& identity) {
    QJsonArray flags;
    for (const std::string& flag : identity.debug_flags) {
        flags.push_back(QString::fromStdString(flag));
    }
    return QJsonObject {
        { QStringLiteral("app"), QString::fromStdString(identity.app) },
        { QStringLiteral("pid"), static_cast<qint64>(identity.pid) },
        { QStringLiteral("build"), QString::fromStdString(identity.build) },
        { QStringLiteral("debug_flags"), flags },
        { QStringLiteral("instrumentation_mode"),
          QString::fromStdString(identity.instrumentation_mode) },
    };
}

QJsonObject descriptor_json(const metric_descriptor& descriptor) {
    QJsonObject value {
        { QStringLiteral("id"), QString::fromStdString(descriptor.id) },
        { QStringLiteral("kind"), QString::fromStdString(descriptor.kind) },
        { QStringLiteral("provenance"),
          QString::fromStdString(descriptor.provenance) },
        { QStringLiteral("unit"), QString::fromStdString(descriptor.unit) },
        { QStringLiteral("scope"), QString::fromStdString(descriptor.scope) },
        { QStringLiteral("cardinality_semantics"),
          QString::fromStdString(descriptor.cardinality_semantics) },
        { QStringLiteral("stability"),
          QString::fromStdString(descriptor.stability) },
        { QStringLiteral("additive_semantics"),
          QString::fromStdString(descriptor.additive_semantics) },
        { QStringLiteral("confidence"),
          QString::fromStdString(descriptor.confidence) },
        { QStringLiteral("default_display_role"),
          QString::fromStdString(descriptor.default_display_role) },
        { QStringLiteral("domain_namespace"),
          QString::fromStdString(descriptor.domain_namespace) },
    };
    if (!descriptor.label.empty()) {
        value.insert(
            QStringLiteral("label"), QString::fromStdString(descriptor.label)
        );
    }
    return value;
}

QJsonValue numeric_json(const numeric_value& value) {
    return std::visit(
        [](const auto number) -> QJsonValue {
            using value_type = std::decay_t<decltype(number)>;
            if constexpr (std::is_same_v<value_type, std::int64_t>) {
                return QJsonValue(static_cast<qint64>(number));
            } else {
                return QJsonValue(number);
            }
        },
        value
    );
}

QJsonArray samples_json(const std::vector<numeric_sample>& samples) {
    QJsonArray values;
    for (const numeric_sample& sample : samples) {
        values.push_back(QJsonObject {
            { QStringLiteral("metric_id"),
              QString::fromStdString(sample.metric_id) },
            { QStringLiteral("value"), numeric_json(sample.value) },
        });
    }
    return values;
}

QByteArray compact(const QJsonObject& value) {
    return QJsonDocument(value).toJson(QJsonDocument::Compact);
}

} // namespace

QByteArray encode_telemetry_json(const hello_record& record) {
    QJsonObject root = header_json(record.header);
    root.insert(QStringLiteral("identity"), identity_json(record.identity));
    QJsonArray catalog;
    for (const metric_descriptor& descriptor : record.metrics) {
        catalog.push_back(descriptor_json(descriptor));
    }
    root.insert(QStringLiteral("metric_catalog"), catalog);
    if (!record.extensions.empty()) {
        QJsonArray extensions;
        for (const std::string& extension : record.extensions) {
            extensions.push_back(QString::fromStdString(extension));
        }
        root.insert(QStringLiteral("extensions"), extensions);
    }
    return compact(root);
}

QByteArray encode_telemetry_json(const sample_batch_record& record) {
    QJsonObject root = header_json(record.header);
    root.insert(QStringLiteral("samples"), samples_json(record.samples));
    return compact(root);
}

QByteArray encode_telemetry_json(const event_batch_record& record) {
    QJsonObject root = header_json(record.header);
    QJsonArray events;
    for (const event_record& event : record.events) {
        QJsonObject value {
            { QStringLiteral("collector_sequence"),
              static_cast<qint64>(event.collector_sequence) },
            { QStringLiteral("kind"),
              QString::fromUtf8(event_kind_token(event.kind)) },
            { QStringLiteral("timestamp_ms"),
              static_cast<qint64>(event.timestamp_ms) },
        };
        if (!event.stream_name.empty()) {
            value.insert(
                QStringLiteral("stream_name"),
                QString::fromStdString(event.stream_name)
            );
        }
        if (!event.line_name.empty()) {
            value.insert(
                QStringLiteral("line_name"),
                QString::fromStdString(event.line_name)
            );
        }
        if (!event.message.empty()) {
            value.insert(
                QStringLiteral("message"),
                QString::fromStdString(event.message)
            );
        }
        if (event.position_x_pct.has_value()
            && event.position_y_pct.has_value()) {
            value.insert(
                QStringLiteral("position_pct"),
                QJsonObject {
                    { QStringLiteral("x"), *event.position_x_pct },
                    { QStringLiteral("y"), *event.position_y_pct },
                }
            );
        }
        events.push_back(value);
    }
    root.insert(QStringLiteral("events"), events);
    return compact(root);
}

QByteArray encode_telemetry_json(const snapshot_record& record) {
    QJsonObject root = header_json(record.header);
    QJsonObject snapshot;
    for (const numeric_sample& sample : record.samples) {
        snapshot.insert(
            QString::fromStdString(sample.metric_id), numeric_json(sample.value)
        );
    }
    snapshot.insert(
        QStringLiteral("reason"), QString::fromStdString(record.reason)
    );
    snapshot.insert(
        QStringLiteral("listener_connected"), record.listener_connected
    );
    snapshot.insert(
        QStringLiteral("endpoint_path"),
        QString::fromStdString(record.endpoint_path)
    );
    root.insert(QStringLiteral("snapshot"), snapshot);
    return compact(root);
}

QByteArray encode_telemetry_json(const marker_record& record) {
    QJsonObject root = header_json(record.header);
    root.insert(
        QStringLiteral("label"), QString::fromStdString(record.label)
    );
    return compact(root);
}

QByteArray encode_telemetry_json(const warning_record& record) {
    QJsonObject root = header_json(record.header);
    root.insert(
        QStringLiteral("warning_code"),
        QString::fromStdString(record.warning_code)
    );
    root.insert(
        QStringLiteral("warning_message"),
        QString::fromStdString(record.warning_message)
    );
    return compact(root);
}

QByteArray encode_telemetry_json(const goodbye_record& record) {
    QJsonObject root = header_json(record.header);
    root.insert(
        QStringLiteral("final_endpoint_path"),
        QString::fromStdString(record.final_endpoint_path)
    );
    return compact(root);
}

} // namespace yodau::data
