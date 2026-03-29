#include "include/main_window_tests.hpp"

#include "shell/frontend_log.hpp"
#include "widgets/settings_panel.hpp"

#include <QComboBox>
#include <QPlainTextEdit>
#include <QTimeZone>
#include <QtTest/QtTest>

namespace main_window_tests_support {

frontend_log_entry make_log_entry(
    const QDateTime& timestamp, const frontend_log_area area,
    const frontend_log_severity severity, const QString& subsystem,
    const QString& message, const QString& stream_name = QString(),
    const QString& detail = QString(), const QString& algorithm_id = QString()
) {
    frontend_log_entry entry;
    entry.timestamp = timestamp;
    entry.area = area;
    entry.severity = severity;
    entry.subsystem = subsystem;
    entry.stream_name = stream_name;
    entry.algorithm_id = algorithm_id;
    entry.message = message;
    entry.detail = detail;
    return entry;
}

} // namespace main_window_tests_support

void main_window_tests::
    format_frontend_log_entry_distinguishes_release_and_debug() {
    const QDateTime timestamp(
        QDate(2026, 3, 29), QTime(3, 4, 5, 67), QTimeZone::UTC
    );
    const frontend_log_entry entry = main_window_tests_support::make_log_entry(
        timestamp, frontend_log_area::active, frontend_log_severity::warning,
        QStringLiteral("backend_event"), QStringLiteral("tripwire triggered"),
        QStringLiteral("cam-a"), QStringLiteral("line=A"),
        QStringLiteral("mask")
    );

    const QString release_text
        = format_frontend_log_entry(frontend_log_mode::release, entry);
    const QString debug_text
        = format_frontend_log_entry(frontend_log_mode::debug, entry);

    QCOMPARE(
        release_text, QStringLiteral("[03:04:05] warn cam-a tripwire triggered")
    );
    QVERIFY(debug_text.contains(QStringLiteral("[03:04:05.067]")));
    QVERIFY(debug_text.contains(QStringLiteral("area=active")));
    QVERIFY(debug_text.contains(QStringLiteral("subsystem=backend_event")));
    QVERIFY(debug_text.contains(QStringLiteral("stream=cam-a")));
    QVERIFY(debug_text.contains(QStringLiteral("alg=mask")));
    QVERIFY(debug_text.contains(QStringLiteral("tripwire triggered")));
    QVERIFY(debug_text.contains(QStringLiteral("detail=line=A")));
}

void main_window_tests::settings_panel_filters_logs_by_area_and_mode() {
    settings_panel panel;
    frontend_log_buffer buffer;

    panel.set_log_buffer(&buffer);

    auto* log_mode_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_log_mode_combo")
    );
    QVERIFY(log_mode_combo != nullptr);
    QCOMPARE(panel.log_mode(), frontend_log_mode::release);
    QCOMPARE(log_mode_combo->currentIndex(), 0);

    auto* add_log_view = panel.findChild<QPlainTextEdit*>(
        QStringLiteral("settings_add_log_view")
    );
    auto* streams_log_view = panel.findChild<QPlainTextEdit*>(
        QStringLiteral("settings_streams_log_view")
    );
    auto* active_log_view = panel.findChild<QPlainTextEdit*>(
        QStringLiteral("settings_active_log_view")
    );

    QVERIFY(add_log_view != nullptr);
    QVERIFY(streams_log_view != nullptr);
    QVERIFY(active_log_view != nullptr);

    const QDateTime timestamp(
        QDate(2026, 3, 29), QTime(12, 0, 0, 0), QTimeZone::UTC
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, frontend_log_area::add, frontend_log_severity::debug,
            QStringLiteral("tests"), QStringLiteral("debug add"),
            QStringLiteral("cam-1"), QStringLiteral("ignored in release")
        )
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, frontend_log_area::add, frontend_log_severity::info,
            QStringLiteral("tests"), QStringLiteral("info add"),
            QStringLiteral("cam-1"), QStringLiteral("path=/tmp/a.mp4")
        )
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, frontend_log_area::streams,
            frontend_log_severity::warning, QStringLiteral("tests"),
            QStringLiteral("streams warning"), QStringLiteral("cam-2")
        )
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, frontend_log_area::active, frontend_log_severity::error,
            QStringLiteral("tests"), QStringLiteral("active error"),
            QStringLiteral("cam-3")
        )
    );

    const QString add_release_text = add_log_view->toPlainText();
    QVERIFY(add_release_text.contains(QStringLiteral("info add")));
    QVERIFY(!add_release_text.contains(QStringLiteral("debug add")));
    QVERIFY(!add_release_text.contains(QStringLiteral("streams warning")));

    const QString streams_release_text = streams_log_view->toPlainText();
    QVERIFY(streams_release_text.contains(QStringLiteral("streams warning")));
    QVERIFY(!streams_release_text.contains(QStringLiteral("active error")));

    const QString active_release_text = active_log_view->toPlainText();
    QVERIFY(active_release_text.contains(QStringLiteral("active error")));
    QVERIFY(!active_release_text.contains(QStringLiteral("debug add")));

    log_mode_combo->setCurrentIndex(1);

    QCOMPARE(panel.log_mode(), frontend_log_mode::debug);
    const QString add_debug_text = add_log_view->toPlainText();
    QVERIFY(add_debug_text.contains(QStringLiteral("debug add")));
    QVERIFY(add_debug_text.contains(QStringLiteral("area=add")));
    QVERIFY(add_debug_text.contains(QStringLiteral("subsystem=tests")));
    QVERIFY(
        add_debug_text.contains(QStringLiteral("detail=ignored in release"))
    );
}

void main_window_tests::
    settings_panel_filters_logs_by_severity_stream_and_subsystem() {
    settings_panel panel;
    frontend_log_buffer buffer;

    panel.set_log_buffer(&buffer);

    auto* log_mode_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_log_mode_combo")
    );
    auto* severity_filter_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_log_severity_filter_combo")
    );
    auto* stream_filter_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_log_stream_filter_combo")
    );
    auto* subsystem_filter_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_log_subsystem_filter_combo")
    );
    auto* add_log_view = panel.findChild<QPlainTextEdit*>(
        QStringLiteral("settings_add_log_view")
    );
    auto* streams_log_view = panel.findChild<QPlainTextEdit*>(
        QStringLiteral("settings_streams_log_view")
    );
    auto* active_log_view = panel.findChild<QPlainTextEdit*>(
        QStringLiteral("settings_active_log_view")
    );

    QVERIFY(log_mode_combo != nullptr);
    QVERIFY(severity_filter_combo != nullptr);
    QVERIFY(stream_filter_combo != nullptr);
    QVERIFY(subsystem_filter_combo != nullptr);
    QVERIFY(add_log_view != nullptr);
    QVERIFY(streams_log_view != nullptr);
    QVERIFY(active_log_view != nullptr);

    const QDateTime timestamp(
        QDate(2026, 3, 29), QTime(13, 0, 0, 0), QTimeZone::UTC
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, frontend_log_area::add, frontend_log_severity::debug,
            QStringLiteral("settings_panel"), QStringLiteral("debug add"),
            QStringLiteral("cam-1")
        )
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, frontend_log_area::add, frontend_log_severity::info,
            QStringLiteral("settings_panel"), QStringLiteral("info add"),
            QStringLiteral("cam-1")
        )
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, frontend_log_area::streams,
            frontend_log_severity::warning, QStringLiteral("grid_visibility"),
            QStringLiteral("grid warning"), QStringLiteral("cam-2")
        )
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, frontend_log_area::active, frontend_log_severity::info,
            QStringLiteral("backend_event"), QStringLiteral("active info"),
            QStringLiteral("cam-1")
        )
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, frontend_log_area::active, frontend_log_severity::error,
            QStringLiteral("backend_event"), QStringLiteral("active error"),
            QStringLiteral("cam-3")
        )
    );

    const int cam1_index
        = stream_filter_combo->findData(QStringLiteral("cam-1"));
    const int backend_event_index
        = subsystem_filter_combo->findData(QStringLiteral("backend_event"));
    const int error_index = severity_filter_combo->findData(
        static_cast<int>(frontend_log_severity::error)
    );
    const int warning_index = severity_filter_combo->findData(
        static_cast<int>(frontend_log_severity::warning)
    );

    QVERIFY(cam1_index >= 0);
    QVERIFY(backend_event_index >= 0);
    QVERIFY(error_index >= 0);
    QVERIFY(warning_index >= 0);

    log_mode_combo->setCurrentIndex(1);
    stream_filter_combo->setCurrentIndex(cam1_index);

    const QString add_cam1_text = add_log_view->toPlainText();
    QVERIFY(add_cam1_text.contains(QStringLiteral("debug add")));
    QVERIFY(add_cam1_text.contains(QStringLiteral("info add")));

    const QString streams_cam1_text = streams_log_view->toPlainText();
    QVERIFY(!streams_cam1_text.contains(QStringLiteral("grid warning")));

    const QString active_cam1_text = active_log_view->toPlainText();
    QVERIFY(active_cam1_text.contains(QStringLiteral("active info")));
    QVERIFY(!active_cam1_text.contains(QStringLiteral("active error")));

    subsystem_filter_combo->setCurrentIndex(backend_event_index);

    QVERIFY(add_log_view->toPlainText().isEmpty());
    QVERIFY(
        active_log_view->toPlainText().contains(QStringLiteral("active info"))
    );

    stream_filter_combo->setCurrentIndex(0);
    severity_filter_combo->setCurrentIndex(error_index);

    QVERIFY(
        active_log_view->toPlainText().contains(QStringLiteral("active error"))
    );
    QVERIFY(
        !active_log_view->toPlainText().contains(QStringLiteral("active info"))
    );
    QVERIFY(streams_log_view->toPlainText().isEmpty());

    subsystem_filter_combo->setCurrentIndex(0);
    severity_filter_combo->setCurrentIndex(warning_index);

    QVERIFY(
        streams_log_view->toPlainText().contains(QStringLiteral("grid warning"))
    );
    QVERIFY(add_log_view->toPlainText().isEmpty());
    QVERIFY(active_log_view->toPlainText().isEmpty());
}
