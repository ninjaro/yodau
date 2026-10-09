#include "shell/app_settings.hpp"
#include "ui/main_window/fixtures.hpp"
#include "ui/main_window_tests.hpp"
#include "widgets/log_area_view.hpp"
#include "widgets/log_toolbar_panel.hpp"
#include "widgets/settings_panel.hpp"

#include <QComboBox>
#include <QFile>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QtTest/QtTest>

// QtTest discovers these private slots through the meta-object.
// NOLINTBEGIN(readability-convert-member-functions-to-static)

void main_window_tests::log_filters_area_and_mode() {
    settings_panel panel;
    app_log_buffer buffer;

    panel.set_log_buffer(&buffer);
    auto* log_toolbar = panel.take_log_toolbar_widget();
    QVERIFY(log_toolbar != nullptr);

    log_area_view add_log_view(app_log_area::add);
    add_log_view.set_log_toolbar(log_toolbar);
    log_area_view streams_log_view(app_log_area::streams);
    streams_log_view.set_log_toolbar(log_toolbar);
    log_area_view active_log_view(app_log_area::active);
    active_log_view.set_log_toolbar(log_toolbar);

    auto* log_mode_combo = log_toolbar->findChild<QComboBox*>(
        QStringLiteral("settings_log_mode_combo")
    );
    QVERIFY(log_mode_combo != nullptr);
    QCOMPARE(panel.log_mode(), app_log_mode::release);
    QCOMPARE(log_mode_combo->currentIndex(), 0);
    QVERIFY(
        panel.findChild<QPlainTextEdit*>(
            QStringLiteral("settings_add_log_view")
        )
        == nullptr
    );
    QVERIFY(
        panel.findChild<QPlainTextEdit*>(
            QStringLiteral("settings_streams_log_view")
        )
        == nullptr
    );
    QVERIFY(
        panel.findChild<QPlainTextEdit*>(
            QStringLiteral("settings_active_log_view")
        )
        == nullptr
    );

    const QDateTime timestamp(
        QDate(2026, 3, 29), QTime(12, 0, 0, 0), QTimeZone::UTC
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, app_log_area::add, app_log_severity::debug,
            QStringLiteral("tests"), QStringLiteral("debug add"),
            QStringLiteral("cam-1"), QStringLiteral("ignored in release")
        )
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, app_log_area::add, app_log_severity::info,
            QStringLiteral("tests"), QStringLiteral("info add"),
            QStringLiteral("cam-1"), QStringLiteral("path=/tmp/a.mp4")
        )
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, app_log_area::streams, app_log_severity::warning,
            QStringLiteral("tests"), QStringLiteral("streams warning"),
            QStringLiteral("cam-2")
        )
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, app_log_area::active, app_log_severity::error,
            QStringLiteral("tests"), QStringLiteral("active error"),
            QStringLiteral("cam-3")
        )
    );

    const QString add_release_text = add_log_view.toPlainText();
    QVERIFY(add_release_text.contains(QStringLiteral("info add")));
    QVERIFY(!add_release_text.contains(QStringLiteral("debug add")));
    QVERIFY(!add_release_text.contains(QStringLiteral("streams warning")));

    const QString streams_release_text = streams_log_view.toPlainText();
    QVERIFY(streams_release_text.contains(QStringLiteral("streams warning")));
    QVERIFY(!streams_release_text.contains(QStringLiteral("active error")));

    const QString active_release_text = active_log_view.toPlainText();
    QVERIFY(active_release_text.contains(QStringLiteral("active error")));
    QVERIFY(!active_release_text.contains(QStringLiteral("debug add")));

    log_mode_combo->setCurrentIndex(1);

    QCOMPARE(panel.log_mode(), app_log_mode::debug);
    const QString add_debug_text = add_log_view.toPlainText();
    QVERIFY(add_debug_text.contains(QStringLiteral("debug add")));
    QVERIFY(add_debug_text.contains(QStringLiteral("area=add")));
    QVERIFY(add_debug_text.contains(QStringLiteral("subsystem=tests")));
    QVERIFY(
        add_debug_text.contains(QStringLiteral("detail=ignored in release"))
    );
}

void main_window_tests::log_filters_severity_stream_and_subsystem() {
    settings_panel panel;
    app_log_buffer buffer;

    panel.set_log_buffer(&buffer);
    auto* log_toolbar = panel.take_log_toolbar_widget();
    QVERIFY(log_toolbar != nullptr);

    log_area_view add_log_view(app_log_area::add);
    add_log_view.set_log_toolbar(log_toolbar);
    log_area_view streams_log_view(app_log_area::streams);
    streams_log_view.set_log_toolbar(log_toolbar);
    log_area_view active_log_view(app_log_area::active);
    active_log_view.set_log_toolbar(log_toolbar);

    auto* log_mode_combo = log_toolbar->findChild<QComboBox*>(
        QStringLiteral("settings_log_mode_combo")
    );
    auto* severity_filter_combo = log_toolbar->findChild<QComboBox*>(
        QStringLiteral("settings_log_severity_filter_combo")
    );
    auto* line_filter_combo = log_toolbar->findChild<QComboBox*>(
        QStringLiteral("settings_log_line_filter_combo")
    );
    auto* algorithm_filter_combo = log_toolbar->findChild<QComboBox*>(
        QStringLiteral("settings_log_algorithm_filter_combo")
    );
    auto* stream_filter_combo = log_toolbar->findChild<QComboBox*>(
        QStringLiteral("settings_log_stream_filter_combo")
    );
    auto* subsystem_filter_combo = log_toolbar->findChild<QComboBox*>(
        QStringLiteral("settings_log_subsystem_filter_combo")
    );

    QVERIFY(log_mode_combo != nullptr);
    QVERIFY(severity_filter_combo != nullptr);
    QVERIFY(line_filter_combo != nullptr);
    QVERIFY(algorithm_filter_combo != nullptr);
    QVERIFY(stream_filter_combo != nullptr);
    QVERIFY(subsystem_filter_combo != nullptr);

    const QDateTime timestamp(
        QDate(2026, 3, 29), QTime(13, 0, 0, 0), QTimeZone::UTC
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, app_log_area::add, app_log_severity::debug,
            QStringLiteral("settings_panel"), QStringLiteral("debug add"),
            QStringLiteral("cam-1")
        )
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, app_log_area::add, app_log_severity::info,
            QStringLiteral("settings_panel"), QStringLiteral("info add"),
            QStringLiteral("cam-1")
        )
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, app_log_area::streams, app_log_severity::warning,
            QStringLiteral("grid_visibility"), QStringLiteral("grid warning"),
            QStringLiteral("cam-2")
        )
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, app_log_area::active, app_log_severity::info,
            QStringLiteral("core_event"), QStringLiteral("active info"),
            QStringLiteral("cam-1"), QStringLiteral("cells=9"),
            QStringLiteral("spot_grid"), QStringLiteral("north"),
            QStringLiteral("motion")
        )
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, app_log_area::active, app_log_severity::error,
            QStringLiteral("core_event"), QStringLiteral("active error"),
            QStringLiteral("cam-3"), QStringLiteral("line=south"),
            QStringLiteral("contour_mask"), QStringLiteral("south"),
            QStringLiteral("tripwire")
        )
    );

    const int cam1_index
        = stream_filter_combo->findData(QStringLiteral("cam-1"));
    const int core_event_index
        = subsystem_filter_combo->findData(QStringLiteral("core_event"));
    const int north_line_index
        = line_filter_combo->findData(QStringLiteral("north"));
    const int spot_grid_index
        = algorithm_filter_combo->findData(QStringLiteral("spot_grid"));
    const int error_index = severity_filter_combo->findData(
        static_cast<int>(app_log_severity::error)
    );
    const int warning_index = severity_filter_combo->findData(
        static_cast<int>(app_log_severity::warning)
    );

    QVERIFY(cam1_index >= 0);
    QVERIFY(core_event_index >= 0);
    QVERIFY(north_line_index >= 0);
    QVERIFY(spot_grid_index >= 0);
    QVERIFY(error_index >= 0);
    QVERIFY(warning_index >= 0);

    log_mode_combo->setCurrentIndex(1);
    stream_filter_combo->setCurrentIndex(cam1_index);

    const QString add_cam1_text = add_log_view.toPlainText();
    QVERIFY(add_cam1_text.contains(QStringLiteral("debug add")));
    QVERIFY(add_cam1_text.contains(QStringLiteral("info add")));

    const QString streams_cam1_text = streams_log_view.toPlainText();
    QVERIFY(!streams_cam1_text.contains(QStringLiteral("grid warning")));

    const QString active_cam1_text = active_log_view.toPlainText();
    QVERIFY(active_cam1_text.contains(QStringLiteral("active info")));
    QVERIFY(!active_cam1_text.contains(QStringLiteral("active error")));

    line_filter_combo->setCurrentIndex(north_line_index);
    algorithm_filter_combo->setCurrentIndex(spot_grid_index);

    subsystem_filter_combo->setCurrentIndex(core_event_index);

    QVERIFY(add_log_view.toPlainText().isEmpty());
    QVERIFY(
        active_log_view.toPlainText().contains(QStringLiteral("active info"))
    );

    stream_filter_combo->setCurrentIndex(0);
    line_filter_combo->setCurrentIndex(0);
    algorithm_filter_combo->setCurrentIndex(0);
    severity_filter_combo->setCurrentIndex(error_index);

    QVERIFY(
        active_log_view.toPlainText().contains(QStringLiteral("active error"))
    );
    QVERIFY(
        !active_log_view.toPlainText().contains(QStringLiteral("active info"))
    );
    QVERIFY(streams_log_view.toPlainText().isEmpty());

    subsystem_filter_combo->setCurrentIndex(0);
    severity_filter_combo->setCurrentIndex(warning_index);

    QVERIFY(
        streams_log_view.toPlainText().contains(QStringLiteral("grid warning"))
    );
    QVERIFY(add_log_view.toPlainText().isEmpty());
    QVERIFY(active_log_view.toPlainText().isEmpty());
}

void main_window_tests::log_exports_filtered_report() {
    settings_panel panel;
    app_log_buffer buffer;

    panel.set_log_buffer(&buffer);

    auto* log_mode_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_log_mode_combo")
    );
    auto* area_filter_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_log_area_filter_combo")
    );

    QVERIFY(log_mode_combo != nullptr);
    QVERIFY(area_filter_combo != nullptr);

    const QDateTime timestamp(
        QDate(2026, 3, 29), QTime(14, 15, 16, 17), QTimeZone::UTC
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, app_log_area::active, app_log_severity::debug,
            QStringLiteral("core_event"), QStringLiteral("motion detected"),
            QStringLiteral("cam-1"), QStringLiteral("cells=9"),
            QStringLiteral("spot_grid")
        )
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, app_log_area::active, app_log_severity::info,
            QStringLiteral("stream_settings"),
            QStringLiteral("algorithm preference updated"),
            QStringLiteral("cam-1"),
            QStringLiteral("core runtime uses contour_mask"),
            QStringLiteral("contour_mask")
        )
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, app_log_area::streams, app_log_severity::info,
            QStringLiteral("grid_visibility"),
            QStringLiteral("stream shown in grid"), QStringLiteral("cam-2")
        )
    );

    log_mode_combo->setCurrentIndex(1);
    const int active_area_index
        = area_filter_combo->findData(static_cast<int>(app_log_area::active));
    QVERIFY(active_area_index >= 0);
    area_filter_combo->setCurrentIndex(active_area_index);

    const QString report = panel.compose_current_log_report();
    QVERIFY(report.contains(QStringLiteral("yodau log report")));
    QVERIFY(report.contains(QStringLiteral("scope=all mode=debug")));
    QVERIFY(report.contains(QStringLiteral("area_filter=active")));
    QVERIFY(report.contains(QStringLiteral("motion detected")));
    QVERIFY(report.contains(QStringLiteral("algorithm preference updated")));
    QVERIFY(!report.contains(QStringLiteral("stream shown in grid")));

    const QString summary = panel.compose_current_log_summary();
    QVERIFY(summary.contains(QStringLiteral("yodau log summary")));
    QVERIFY(summary.contains(QStringLiteral("scope=all")));
    QVERIFY(summary.contains(QStringLiteral("entries=2")));
    QVERIFY(summary.contains(QStringLiteral("debug=1")));
    QVERIFY(summary.contains(QStringLiteral("info=1")));
    QVERIFY(summary.contains(QStringLiteral("warn=0")));
    QVERIFY(summary.contains(QStringLiteral("stream_list=cam-1")));

    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());

    const QString output_path = temp_dir.filePath(QStringLiteral("report.txt"));
    QVERIFY(panel.write_current_log_report(output_path));

    QFile report_file(output_path);
    QVERIFY(report_file.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString::fromUtf8(report_file.readAll()), report);
}

// NOLINTEND(readability-convert-member-functions-to-static)
