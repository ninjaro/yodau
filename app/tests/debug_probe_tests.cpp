#include "include/debug_probe_tests.hpp"

#include "monitor/debug_probe.hpp"
#include "monitor/runtime_bridge.hpp"
#include "streams/stream_manager.hpp"

#include <QElapsedTimer>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QSet>
#include <QtTest/QtTest>

#include <algorithm>

using yodau::monitor::debug_probe;
using yodau::monitor::runtime_bridge;

QVector<QJsonObject> debug_probe_tests::read_protocol_messages(
    QLocalSocket& socket, int minimum_messages, int timeout_ms
) {
    QVector<QJsonObject> messages;
    QByteArray buffer;
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeout_ms && messages.size() < minimum_messages) {
        if (socket.bytesAvailable() == 0
            && !socket.waitForReadyRead(
                std::max(1, timeout_ms - static_cast<int>(timer.elapsed()))
            )) {
            continue;
        }

        buffer.append(socket.readAll());
        qsizetype newline_index = buffer.indexOf('\n');
        while (newline_index >= 0) {
            const QByteArray line = buffer.left(newline_index).trimmed();
            buffer.remove(0, newline_index + 1);
            if (!line.isEmpty()) {
                const QJsonDocument document = QJsonDocument::fromJson(line);
                if (document.isObject()) {
                    messages.push_back(document.object());
                }
            }
            newline_index = buffer.indexOf('\n');
        }
    }

    return messages;
}

QJsonObject debug_probe_tests::find_message_by_family(
    const QVector<QJsonObject>& messages, const QString& family
) {
    for (const QJsonObject& message : messages) {
        const QJsonObject protocol
            = message.value(QStringLiteral("protocol_v1")).toObject();
        if (protocol.value(QStringLiteral("message_family")).toString()
            == family) {
            return message;
        }
    }
    return QJsonObject();
}

void debug_probe_tests::protocol_capabilities_expose_metric_catalog() {
    const QJsonObject capabilities = debug_probe::protocol_capabilities_v1();
    QVERIFY(capabilities.contains(QStringLiteral("metric_catalog")));

    const QJsonArray catalog
        = capabilities.value(QStringLiteral("metric_catalog")).toArray();
    QVERIFY(catalog.size() >= 6);

    const QJsonObject first_metric = catalog.first().toObject();
    QCOMPARE(
        first_metric.value(QStringLiteral("id")).toString(),
        QStringLiteral("process_memory_rss_bytes")
    );
}

void debug_probe_tests::protocol_message_wraps_identity_and_payload() {
    const debug_probe::protocol_identity identity {
        .app_name = QStringLiteral("yodau"),
        .process_id = 42,
        .session_id = QStringLiteral("session-1"),
        .build_id = QStringLiteral("dev"),
        .protocol_version = debug_probe::protocol_version_string(),
        .debug_flags = { QStringLiteral("debug_build") },
        .instrumentation_mode = QStringLiteral("debug_runtime"),
    };

    QJsonObject payload;
    payload.insert(
        QStringLiteral("snapshot"),
        QJsonObject {
            { QStringLiteral("foo"), 1 },
        }
    );
    const QJsonObject message = debug_probe::build_protocol_message_v1(
        QStringLiteral("snapshot"), identity, 1234, payload
    );

    QCOMPARE(
        message.value(QStringLiteral("monotonic_timestamp_ms")).toInteger(),
        qint64(1234)
    );
    const QJsonObject protocol
        = message.value(QStringLiteral("protocol_v1")).toObject();
    QCOMPARE(
        protocol.value(QStringLiteral("message_family")).toString(),
        QStringLiteral("snapshot")
    );

    const QJsonObject identity_json
        = protocol.value(QStringLiteral("identity")).toObject();
    QCOMPARE(
        identity_json.value(QStringLiteral("app")).toString(),
        QStringLiteral("yodau")
    );
    QCOMPARE(
        identity_json.value(QStringLiteral("pid")).toInteger(), qint64(42)
    );
}

void debug_probe_tests::runtime_bridge_enables_local_ipc_endpoint() {
#if defined(NDEBUG)
    QSKIP(
        "Debug monitor broadcasting is intentionally disabled in release "
        "builds."
    );
#else
    runtime_bridge runtime(
        runtime_bridge::runtime_options {
            .enabled = false,
            .requested_endpoint = QStringLiteral("yodau_probe_test"),
        }
    );
    if (!runtime.set_enabled(true)) {
        QSKIP("QLocalServer::listen is unavailable in this environment.");
    }
    QVERIFY(runtime.is_enabled());

    const QString endpoint_path = runtime.endpoint_path();
    QVERIFY(!endpoint_path.isEmpty());
    QLocalSocket socket;
    socket.connectToServer(endpoint_path);
    QVERIFY2(
        socket.waitForConnected(1000),
        qPrintable(
            QStringLiteral("expected IPC endpoint at %1").arg(endpoint_path)
        )
    );

    QVERIFY(runtime.set_enabled(false));
    QVERIFY(!runtime.is_enabled());
#endif
}

void debug_probe_tests::runtime_bridge_publishes_initial_protocol_messages() {
#if defined(NDEBUG)
    QSKIP(
        "Debug monitor broadcasting is intentionally disabled in release "
        "builds."
    );
#else
    runtime_bridge runtime(
        runtime_bridge::runtime_options {
            .enabled = false,
            .requested_endpoint = QStringLiteral("yodau_probe_bootstrap"),
        }
    );
    if (!runtime.set_enabled(true)) {
        QSKIP("QLocalServer::listen is unavailable in this environment.");
    }
    runtime.set_inventory(4, 3, 2, 6, 1);

    QLocalSocket socket;
    socket.connectToServer(runtime.endpoint_path());
    QVERIFY(socket.waitForConnected(1000));

    const QVector<QJsonObject> messages
        = read_protocol_messages(socket, 5, 2000);
    QVERIFY2(
        messages.size() >= 4, "expected initial monitor bootstrap traffic"
    );

    QSet<QString> families;
    for (const QJsonObject& message : messages) {
        const QJsonObject protocol
            = message.value(QStringLiteral("protocol_v1")).toObject();
        families.insert(
            protocol.value(QStringLiteral("message_family")).toString()
        );
        const QJsonObject identity
            = protocol.value(QStringLiteral("identity")).toObject();
        QCOMPARE(
            identity.value(QStringLiteral("app")).toString(),
            QStringLiteral("yodau")
        );
        QCOMPARE(
            protocol.value(QStringLiteral("version")).toString(),
            debug_probe::protocol_version_string()
        );
    }

    QVERIFY(families.contains(QStringLiteral("hello")));
    QVERIFY(families.contains(QStringLiteral("capabilities")));
    QVERIFY(families.contains(QStringLiteral("sample_batch")));
    QVERIFY(families.contains(QStringLiteral("snapshot")));

    const QJsonObject capabilities_message
        = find_message_by_family(messages, QStringLiteral("capabilities"));
    QVERIFY(!capabilities_message.isEmpty());
    const QJsonArray catalog
        = capabilities_message.value(QStringLiteral("capabilities"))
              .toObject()
              .value(QStringLiteral("metric_catalog"))
              .toArray();
    QVERIFY(catalog.size() >= 6);

    const QJsonObject sample_batch
        = find_message_by_family(messages, QStringLiteral("sample_batch"));
    QVERIFY(!sample_batch.isEmpty());
    const QJsonArray samples
        = sample_batch.value(QStringLiteral("samples")).toArray();
    QVERIFY(samples.size() >= 7);

    QSet<QString> sample_ids;
    for (const QJsonValue& sample_value : samples) {
        sample_ids.insert(sample_value.toObject()
                              .value(QStringLiteral("metric_id"))
                              .toString());
    }
    QVERIFY(sample_ids.contains(QStringLiteral("configured_stream_count")));
    QVERIFY(sample_ids.contains(QStringLiteral("visible_stream_count")));
    QVERIFY(sample_ids.contains(QStringLiteral("active_stream_count")));
    QVERIFY(sample_ids.contains(QStringLiteral("configured_line_count")));
    QVERIFY(sample_ids.contains(QStringLiteral("detected_local_source_count")));

    const QJsonObject snapshot_message
        = find_message_by_family(messages, QStringLiteral("snapshot"));
    QVERIFY(!snapshot_message.isEmpty());
    const QJsonObject snapshot
        = snapshot_message.value(QStringLiteral("snapshot")).toObject();
    QCOMPARE(
        snapshot.value(QStringLiteral("configured_stream_count")).toInt(), 4
    );
    QCOMPARE(snapshot.value(QStringLiteral("visible_stream_count")).toInt(), 3);
    QCOMPARE(snapshot.value(QStringLiteral("active_stream_count")).toInt(), 2);
    QCOMPARE(
        snapshot.value(QStringLiteral("configured_line_count")).toInt(), 6
    );
    QCOMPARE(
        snapshot.value(QStringLiteral("detected_local_source_count")).toInt(), 1
    );
    QCOMPARE(
        snapshot.value(QStringLiteral("endpoint_path")).toString(),
        runtime.endpoint_path()
    );
#endif
}

void debug_probe_tests::
    runtime_bridge_event_batch_updates_counts_and_payload() {
#if defined(NDEBUG)
    QSKIP(
        "Debug monitor broadcasting is intentionally disabled in release "
        "builds."
    );
#else
    runtime_bridge runtime(
        runtime_bridge::runtime_options {
            .enabled = false,
            .requested_endpoint = QStringLiteral("yodau_probe_events"),
        }
    );
    if (!runtime.set_enabled(true)) {
        QSKIP("QLocalServer::listen is unavailable in this environment.");
    }

    QLocalSocket socket;
    socket.connectToServer(runtime.endpoint_path());
    QVERIFY(socket.waitForConnected(1000));
    // Drain bootstrap traffic before asserting on the explicit event batch.
    (void)read_protocol_messages(socket, 4, 2000);

    std::vector<yodau::backend::event> events;
    yodau::backend::event motion_event;
    motion_event.kind = yodau::backend::event_kind::motion;
    motion_event.stream_name = "cam-a";
    motion_event.line_name = "tripwire-1";
    motion_event.message = "motion detected";
    motion_event.ts = std::chrono::steady_clock::now();
    motion_event.pos_pct = yodau::backend::point { 12.5f, 44.0f };
    events.push_back(motion_event);

    yodau::backend::event tripwire_event;
    tripwire_event.kind = yodau::backend::event_kind::tripwire;
    tripwire_event.stream_name = "cam-b";
    tripwire_event.message = "tripwire crossed";
    tripwire_event.ts = std::chrono::steady_clock::now();
    events.push_back(tripwire_event);

    runtime.record_event_batch(events);

    const QVector<QJsonObject> messages
        = read_protocol_messages(socket, 2, 2000);
    QVERIFY(messages.size() >= 2);

    const QJsonObject event_batch
        = find_message_by_family(messages, QStringLiteral("event_batch"));
    QVERIFY(!event_batch.isEmpty());
    const QJsonArray encoded_events
        = event_batch.value(QStringLiteral("events")).toArray();
    QCOMPARE(encoded_events.size(), 2);

    const QJsonObject first_event = encoded_events.at(0).toObject();
    QCOMPARE(
        first_event.value(QStringLiteral("kind")).toString(),
        QStringLiteral("motion")
    );
    QCOMPARE(
        first_event.value(QStringLiteral("stream_name")).toString(),
        QStringLiteral("cam-a")
    );
    QCOMPARE(
        first_event.value(QStringLiteral("line_name")).toString(),
        QStringLiteral("tripwire-1")
    );
    QCOMPARE(
        first_event.value(QStringLiteral("message")).toString(),
        QStringLiteral("motion detected")
    );
    const QJsonObject first_pos
        = first_event.value(QStringLiteral("position_pct")).toObject();
    QCOMPARE(first_pos.value(QStringLiteral("x")).toDouble(), 12.5);
    QCOMPARE(first_pos.value(QStringLiteral("y")).toDouble(), 44.0);

    const QJsonObject second_event = encoded_events.at(1).toObject();
    QCOMPARE(
        second_event.value(QStringLiteral("kind")).toString(),
        QStringLiteral("tripwire")
    );
    QCOMPARE(
        second_event.value(QStringLiteral("stream_name")).toString(),
        QStringLiteral("cam-b")
    );
    QCOMPARE(
        second_event.value(QStringLiteral("message")).toString(),
        QStringLiteral("tripwire crossed")
    );

    const QJsonObject sample_batch
        = find_message_by_family(messages, QStringLiteral("sample_batch"));
    QVERIFY(!sample_batch.isEmpty());
    const QJsonArray samples
        = sample_batch.value(QStringLiteral("samples")).toArray();

    qint64 motion_count = -1;
    qint64 tripwire_count = -1;
    for (const QJsonValue& sample_value : samples) {
        const QJsonObject sample = sample_value.toObject();
        const QString metric_id
            = sample.value(QStringLiteral("metric_id")).toString();
        if (metric_id == QStringLiteral("motion_event_count")) {
            motion_count = sample.value(QStringLiteral("value")).toInteger();
        } else if (metric_id == QStringLiteral("tripwire_event_count")) {
            tripwire_count = sample.value(QStringLiteral("value")).toInteger();
        }
    }

    QCOMPARE(motion_count, qint64(1));
    QCOMPARE(tripwire_count, qint64(1));
#endif
}
