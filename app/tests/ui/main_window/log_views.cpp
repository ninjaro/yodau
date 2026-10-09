#include "shell/app_settings.hpp"
#include "shell/processing_feedback_state.hpp"
#include "ui/main_window/fixtures.hpp"
#include "ui/main_window_tests.hpp"
#include "widgets/log_area_view.hpp"
#include "widgets/log_toolbar_panel.hpp"
#include "widgets/stream_source_panel.hpp"

#include <QComboBox>
#include <QFile>
#include <QJsonDocument>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QtTest/QtTest>

// QtTest discovers these private slots through the meta-object.
// NOLINTBEGIN(readability-convert-member-functions-to-static)

void main_window_tests::log_toolbar_filters_formats_and_actions() {
    log_toolbar_panel panel;
    app_log_buffer buffer;
    panel.set_log_buffer(&buffer);

    QSignalSpy copy_logs_spy(&panel, &log_toolbar_panel::copy_logs_requested);
    QSignalSpy copy_summary_spy(
        &panel, &log_toolbar_panel::copy_summary_requested
    );
    QSignalSpy save_logs_spy(&panel, &log_toolbar_panel::save_logs_requested);

    auto* log_mode_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_log_mode_combo")
    );
    auto* area_filter_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_log_area_filter_combo")
    );
    auto* severity_filter_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_log_severity_filter_combo")
    );
    auto* event_filter_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_log_event_filter_combo")
    );
    auto* line_filter_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_log_line_filter_combo")
    );
    auto* algorithm_filter_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_log_algorithm_filter_combo")
    );
    auto* stream_filter_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_log_stream_filter_combo")
    );
    auto* subsystem_filter_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_log_subsystem_filter_combo")
    );
    auto* search_filter_edit = panel.findChild<QLineEdit*>(
        QStringLiteral("settings_log_search_filter_edit")
    );
    auto* copy_logs_button = panel.findChild<QPushButton*>(
        QStringLiteral("settings_copy_logs_button")
    );
    auto* copy_summary_button = panel.findChild<QPushButton*>(
        QStringLiteral("settings_copy_summary_button")
    );
    auto* save_logs_button = panel.findChild<QPushButton*>(
        QStringLiteral("settings_save_logs_button")
    );

    QVERIFY(log_mode_combo != nullptr);
    QVERIFY(area_filter_combo != nullptr);
    QVERIFY(severity_filter_combo != nullptr);
    QVERIFY(event_filter_combo != nullptr);
    QVERIFY(line_filter_combo != nullptr);
    QVERIFY(algorithm_filter_combo != nullptr);
    QVERIFY(stream_filter_combo != nullptr);
    QVERIFY(subsystem_filter_combo != nullptr);
    QVERIFY(search_filter_edit != nullptr);
    QVERIFY(copy_logs_button != nullptr);
    QVERIFY(copy_summary_button != nullptr);
    QVERIFY(save_logs_button != nullptr);
    QCOMPARE(panel.log_mode(), app_log_mode::release);

    const QDateTime timestamp(
        QDate(2026, 3, 29), QTime(14, 0, 0, 0), QTimeZone::UTC
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, app_log_area::add, app_log_severity::debug,
            QStringLiteral("tests"), QStringLiteral("debug add"),
            QStringLiteral("cam-1"), QStringLiteral("hidden in release")
        )
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, app_log_area::add, app_log_severity::info,
            QStringLiteral("tests"), QStringLiteral("info add"),
            QStringLiteral("cam-1")
        )
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, app_log_area::active, app_log_severity::info,
            QStringLiteral("core_event"), QStringLiteral("active info"),
            QStringLiteral("cam-1"), QStringLiteral("cells=9"),
            QStringLiteral("spot_grid"), QStringLiteral("north"),
            QStringLiteral("motion"), QColor(QStringLiteral("#67c1ff"))
        )
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, app_log_area::active, app_log_severity::error,
            QStringLiteral("core_event"), QStringLiteral("active error"),
            QStringLiteral("cam-2"), QStringLiteral("core=line=south"),
            QStringLiteral("contour_mask"), QStringLiteral("south"),
            QStringLiteral("tripwire"), QColor(QStringLiteral("#ff4d6d"))
        )
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, app_log_area::streams, app_log_severity::warning,
            QStringLiteral("grid_visibility"),
            QStringLiteral("streams warning"), QStringLiteral("cam-2"),
            QStringLiteral("roi window"), QString(), QStringLiteral("poly"),
            QStringLiteral("roi"), QColor(QStringLiteral("#2a9d8f"))
        )
    );

    const QString add_release_text
        = panel.formatted_entries(app_log_area::add).join('\n');
    QVERIFY(add_release_text.contains(QStringLiteral("info add")));
    QVERIFY(!add_release_text.contains(QStringLiteral("debug add")));

    log_mode_combo->setCurrentIndex(1);
    QCOMPARE(panel.log_mode(), app_log_mode::debug);
    const QString add_debug_text
        = panel.formatted_entries(app_log_area::add).join('\n');
    QVERIFY(add_debug_text.contains(QStringLiteral("debug add")));
    QVERIFY(add_debug_text.contains(QStringLiteral("area=add")));

    const int core_event_index
        = subsystem_filter_combo->findData(QStringLiteral("core_event"));
    const int active_area_index
        = area_filter_combo->findData(static_cast<int>(app_log_area::active));
    const int tripwire_index
        = event_filter_combo->findData(QStringLiteral("tripwire"));
    const int south_line_index
        = line_filter_combo->findData(QStringLiteral("south"));
    const int contour_mask_index
        = algorithm_filter_combo->findData(QStringLiteral("contour_mask"));
    const int cam2_index
        = stream_filter_combo->findData(QStringLiteral("cam-2"));
    const int error_index = severity_filter_combo->findData(
        static_cast<int>(app_log_severity::error)
    );
    QVERIFY(core_event_index >= 0);
    QVERIFY(active_area_index >= 0);
    QVERIFY(tripwire_index >= 0);
    QVERIFY(south_line_index >= 0);
    QVERIFY(contour_mask_index >= 0);
    QVERIFY(cam2_index >= 0);
    QVERIFY(error_index >= 0);

    area_filter_combo->setCurrentIndex(active_area_index);
    event_filter_combo->setCurrentIndex(tripwire_index);
    line_filter_combo->setCurrentIndex(south_line_index);
    algorithm_filter_combo->setCurrentIndex(contour_mask_index);
    subsystem_filter_combo->setCurrentIndex(core_event_index);
    stream_filter_combo->setCurrentIndex(cam2_index);
    severity_filter_combo->setCurrentIndex(error_index);
    search_filter_edit->setText(QStringLiteral("south"));

    const QString active_error_report = panel.compose_log_report(std::nullopt);
    QVERIFY(active_error_report.contains(QStringLiteral("active error")));
    QVERIFY(!active_error_report.contains(QStringLiteral("active info")));
    QVERIFY(!active_error_report.contains(QStringLiteral("streams warning")));
    QVERIFY(active_error_report.contains(QStringLiteral("scope=all")));
    QVERIFY(active_error_report.contains(QStringLiteral("area_filter=active")));
    QVERIFY(active_error_report.contains(QStringLiteral("line=south")));
    QVERIFY(
        active_error_report.contains(QStringLiteral("algorithm=contour_mask"))
    );

    const QString active_error_summary
        = panel.compose_log_summary(std::nullopt);
    QVERIFY(active_error_summary.contains(QStringLiteral("entries=1")));
    QVERIFY(active_error_summary.contains(QStringLiteral("error=1")));
    QVERIFY(
        active_error_summary.contains(QStringLiteral("event_list=tripwire"))
    );
    QVERIFY(active_error_summary.contains(QStringLiteral("line_list=south")));
    QVERIFY(active_error_summary.contains(
        QStringLiteral("algorithm_list=contour_mask")
    ));

    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());
    const QString report_path
        = temp_dir.filePath(QStringLiteral("log-report.txt"));
    QVERIFY(panel.write_log_report(std::nullopt, report_path));

    QFile saved_report(report_path);
    QVERIFY(saved_report.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString saved_text = QString::fromUtf8(saved_report.readAll());
    QVERIFY(saved_text.contains(QStringLiteral("active error")));

    const QString tsv_path
        = temp_dir.filePath(QStringLiteral("log-report.tsv"));
    QVERIFY(panel.write_log_report(std::nullopt, tsv_path));
    QFile saved_tsv(tsv_path);
    QVERIFY(saved_tsv.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString tsv_text = QString::fromUtf8(saved_tsv.readAll());
    QVERIFY(tsv_text.startsWith(QStringLiteral("timestamp\tarea\tseverity")));
    QVERIFY(tsv_text.contains(QStringLiteral("tripwire")));

    const QString json_path
        = temp_dir.filePath(QStringLiteral("log-report.json"));
    QVERIFY(panel.write_log_report(std::nullopt, json_path));
    QFile saved_json(json_path);
    QVERIFY(saved_json.open(QIODevice::ReadOnly));
    const QJsonDocument json_doc
        = QJsonDocument::fromJson(saved_json.readAll());
    QVERIFY(json_doc.isObject());
    QVERIFY(
        json_doc.object().value(QStringLiteral("entry_count")).toInt() == 1
    );
    QVERIFY(json_doc.toJson(QJsonDocument::Compact)
                .contains("\"event_type\":\"tripwire\""));

    copy_logs_button->click();
    copy_summary_button->click();
    save_logs_button->click();
    QCOMPARE(copy_logs_spy.size(), 1);
    QCOMPARE(copy_summary_spy.size(), 1);
    QCOMPARE(save_logs_spy.size(), 1);
}

void main_window_tests::log_view_refresh_and_fallback() {
    app_log_buffer buffer;
    log_toolbar_panel toolbar;
    toolbar.set_log_buffer(&buffer);

    log_area_view active_view(app_log_area::active);
    active_view.set_log_toolbar(&toolbar);
    log_area_view global_view(std::nullopt);
    global_view.set_log_toolbar(&toolbar);

    auto* area_filter_combo = toolbar.findChild<QComboBox*>(
        QStringLiteral("settings_log_area_filter_combo")
    );
    auto* event_filter_combo = toolbar.findChild<QComboBox*>(
        QStringLiteral("settings_log_event_filter_combo")
    );
    auto* line_filter_combo = toolbar.findChild<QComboBox*>(
        QStringLiteral("settings_log_line_filter_combo")
    );
    auto* algorithm_filter_combo = toolbar.findChild<QComboBox*>(
        QStringLiteral("settings_log_algorithm_filter_combo")
    );
    auto* stream_filter_combo = toolbar.findChild<QComboBox*>(
        QStringLiteral("settings_log_stream_filter_combo")
    );
    auto* search_filter_edit = toolbar.findChild<QLineEdit*>(
        QStringLiteral("settings_log_search_filter_edit")
    );
    QVERIFY(area_filter_combo != nullptr);
    QVERIFY(event_filter_combo != nullptr);
    QVERIFY(line_filter_combo != nullptr);
    QVERIFY(algorithm_filter_combo != nullptr);
    QVERIFY(stream_filter_combo != nullptr);
    QVERIFY(search_filter_edit != nullptr);

    const QDateTime timestamp(
        QDate(2026, 3, 29), QTime(14, 5, 6, 7), QTimeZone::UTC
    );
    const app_log_entry active_debug
        = main_window_tests_support::make_log_entry(
            timestamp, app_log_area::active, app_log_severity::debug,
            QStringLiteral("core_event"), QStringLiteral("debug motion"),
            QStringLiteral("cam-1"), QStringLiteral("cells=9"),
            QStringLiteral("spot_grid"), QStringLiteral("north"),
            QStringLiteral("motion"), QColor(QStringLiteral("#67c1ff"))
        );
    const app_log_entry active_warning
        = main_window_tests_support::make_log_entry(
            timestamp, app_log_area::active, app_log_severity::warning,
            QStringLiteral("core_event"), QStringLiteral("tripwire warning"),
            QStringLiteral("cam-2"), QStringLiteral("line=south"),
            QStringLiteral("contour_mask"), QStringLiteral("south"),
            QStringLiteral("tripwire"), QColor(QStringLiteral("#ff4d6d"))
        );
    const app_log_entry streams_error
        = main_window_tests_support::make_log_entry(
            timestamp, app_log_area::streams, app_log_severity::error,
            QStringLiteral("grid_visibility"), QStringLiteral("streams error"),
            QStringLiteral("cam-2"), QStringLiteral("roi"), QString(),
            QStringLiteral("poly"), QStringLiteral("roi"),
            QColor(QStringLiteral("#2a9d8f"))
        );

    buffer.append(active_debug);
    buffer.append(active_warning);
    buffer.append(streams_error);

    const QString active_release_text = active_view.toPlainText();
    QVERIFY(!active_release_text.contains(QStringLiteral("debug motion")));
    QVERIFY(active_release_text.contains(QStringLiteral("tripwire warning")));
    QVERIFY(!active_release_text.contains(QStringLiteral("streams error")));
    const QString global_release_text = global_view.toPlainText();
    QVERIFY(global_release_text.contains(QStringLiteral("streams error")));
    QVERIFY(global_release_text.contains(QStringLiteral("tripwire")));

    toolbar.set_log_mode(app_log_mode::debug);
    const QString active_debug_text = active_view.toPlainText();
    QVERIFY(active_debug_text.contains(QStringLiteral("debug motion")));
    QVERIFY(active_debug_text.contains(QStringLiteral("area=active")));
    QVERIFY(!active_debug_text.contains(QStringLiteral("streams error")));

    const int cam2_index
        = stream_filter_combo->findData(QStringLiteral("cam-2"));
    QVERIFY(cam2_index >= 0);
    stream_filter_combo->setCurrentIndex(cam2_index);
    const QString cam2_text = active_view.toPlainText();
    QVERIFY(!cam2_text.contains(QStringLiteral("debug motion")));
    QVERIFY(cam2_text.contains(QStringLiteral("tripwire warning")));

    const int tripwire_index
        = event_filter_combo->findData(QStringLiteral("tripwire"));
    const int south_line_index
        = line_filter_combo->findData(QStringLiteral("south"));
    const int contour_mask_index
        = algorithm_filter_combo->findData(QStringLiteral("contour_mask"));
    QVERIFY(tripwire_index >= 0);
    QVERIFY(south_line_index >= 0);
    QVERIFY(contour_mask_index >= 0);
    event_filter_combo->setCurrentIndex(tripwire_index);
    line_filter_combo->setCurrentIndex(south_line_index);
    algorithm_filter_combo->setCurrentIndex(contour_mask_index);
    search_filter_edit->setText(QStringLiteral("south"));
    const QString filtered_global_text = global_view.toPlainText();
    QVERIFY(filtered_global_text.contains(QStringLiteral("tripwire warning")));
    QVERIFY(!filtered_global_text.contains(QStringLiteral("streams error")));

    const int streams_area_index
        = area_filter_combo->findData(static_cast<int>(app_log_area::streams));
    QVERIFY(streams_area_index >= 0);
    area_filter_combo->setCurrentIndex(streams_area_index);
    search_filter_edit->clear();
    event_filter_combo->setCurrentIndex(0);
    line_filter_combo->setCurrentIndex(0);
    algorithm_filter_combo->setCurrentIndex(0);
    const QString streams_only_text = global_view.toPlainText();
    QVERIFY(streams_only_text.contains(QStringLiteral("streams error")));
    QVERIFY(!streams_only_text.contains(QStringLiteral("tripwire warning")));

    log_area_view add_view(app_log_area::add);
    const app_log_entry add_warning = main_window_tests_support::make_log_entry(
        timestamp, app_log_area::add, app_log_severity::warning,
        QStringLiteral("stream_source_panel"), QStringLiteral("invalid url"),
        QStringLiteral("cam-url"), QStringLiteral("missing scheme"), QString(),
        QStringLiteral("draft"), QStringLiteral("info"),
        QColor(QStringLiteral("#999999"))
    );

    QVERIFY(add_view.append_entry(add_warning));
    QCOMPARE(
        add_view.toPlainText(),
        format_app_log_entry(app_log_mode::release, add_warning)
    );
    QVERIFY(add_view.toPlainText().contains(QStringLiteral("line=draft")));
    QVERIFY(!add_view.append_entry(active_warning));
    QCOMPARE(
        add_view.toPlainText(),
        format_app_log_entry(app_log_mode::release, add_warning)
    );
}

void main_window_tests::log_release_and_debug_format() {
    const QDateTime timestamp(
        QDate(2026, 3, 29), QTime(3, 4, 5, 67), QTimeZone::UTC
    );
    const app_log_entry entry = main_window_tests_support::make_log_entry(
        timestamp, app_log_area::active, app_log_severity::warning,
        QStringLiteral("core_event"), QStringLiteral("tripwire triggered"),
        QStringLiteral("cam-a"), QStringLiteral("line=A"),
        QStringLiteral("mask"), QStringLiteral("A"), QStringLiteral("tripwire"),
        QColor(Qt::red)
    );

    const QString release_text
        = format_app_log_entry(app_log_mode::release, entry);
    const QString debug_text = format_app_log_entry(app_log_mode::debug, entry);

    QCOMPARE(
        release_text,
        QStringLiteral(
            "[03:04:05] warn cam-a tripwire line=A tripwire triggered"
        )
    );
    QVERIFY(debug_text.contains(QStringLiteral("[03:04:05.067]")));
    QVERIFY(debug_text.contains(QStringLiteral("area=active")));
    QVERIFY(debug_text.contains(QStringLiteral("subsystem=core_event")));
    QVERIFY(debug_text.contains(QStringLiteral("stream=cam-a")));
    QVERIFY(debug_text.contains(QStringLiteral("line=A")));
    QVERIFY(debug_text.contains(QStringLiteral("alg=mask")));
    QVERIFY(debug_text.contains(QStringLiteral("event=tripwire")));
    QVERIFY(debug_text.contains(QStringLiteral("tripwire triggered")));
    QVERIFY(debug_text.contains(QStringLiteral("detail=line=A")));
}

void main_window_tests::log_bounds_and_incremental_updates() {
    app_log_buffer buffer;
    buffer.set_capacity(8);
    QCOMPARE(buffer.capacity(), qsizetype(8));

    log_toolbar_panel toolbar;
    toolbar.set_log_buffer(&buffer);
    log_area_view view;
    view.set_log_toolbar(&toolbar);
    QSignalSpy prune_spy(&buffer, &app_log_buffer::entries_pruned);

    for (int index = 0; index < 10; ++index) {
        buffer.append(
            main_window_tests_support::make_log_entry(
                QDateTime::currentDateTime(), app_log_area::active,
                app_log_severity::info, QStringLiteral("bounded"),
                QStringLiteral("entry-%1").arg(index),
                QStringLiteral("camera-%1").arg(index % 2)
            )
        );
    }

    QCOMPARE(buffer.entries().size(), qsizetype(8));
    QVERIFY(!prune_spy.isEmpty());
    QCOMPARE(buffer.entries().front().message, QStringLiteral("entry-2"));
    QCOMPARE(view.visible_entries().size(), buffer.entries().size());
    QVERIFY(view.toPlainText().contains(QStringLiteral("entry-9")));

    auto* stream_filter = toolbar.findChild<QComboBox*>(
        QStringLiteral("settings_log_stream_filter_combo")
    );
    QVERIFY(stream_filter != nullptr);
    QVERIFY(stream_filter->findData(QStringLiteral("camera-0")) >= 0);
    QVERIFY(stream_filter->findData(QStringLiteral("camera-1")) >= 0);
}

void main_window_tests::feedback_log_details_and_motion_throttle() {
    processing_feedback_state feedback_state;

    yodau::core::event tripwire_event;
    tripwire_event.kind = yodau::core::event_kind::tripwire;
    tripwire_event.stream_name = "cam-1";
    tripwire_event.line_name = "north";
    tripwire_event.message = "north|0.5|1.25";
    tripwire_event.pos_pct = yodau::core::point { 25.0f, 75.0f };

    const auto tripwire_feedback = feedback_state.consume_event(tripwire_event);
    QCOMPARE(tripwire_feedback.kind_text, QStringLiteral("tripwire"));
    QCOMPARE(
        tripwire_feedback.log_message, QStringLiteral("tripwire triggered")
    );
    QCOMPARE(tripwire_feedback.log_severity, app_log_severity::info);
    QVERIFY(tripwire_feedback.allow_gui_overlay);
    QVERIFY(tripwire_feedback.overlay_position_pct.has_value());
    QCOMPARE(
        tripwire_feedback.overlay_position_pct.value(), QPointF(25.0, 75.0)
    );
    QCOMPARE(
        tripwire_feedback.overlay_color, QColor(QStringLiteral("#e63946"))
    );
    QVERIFY(tripwire_feedback.tripwire_visual.has_value());
    QCOMPARE(
        tripwire_feedback.tripwire_visual->direction, QStringLiteral("north")
    );
    QCOMPARE(tripwire_feedback.tripwire_visual->strength, 0.5);
    QCOMPARE(tripwire_feedback.tripwire_visual->speed, 1.25);
    QVERIFY(
        tripwire_feedback.log_detail.contains(QStringLiteral("line=north"))
    );
    QVERIFY(tripwire_feedback.log_detail.contains(
        QStringLiteral("pos=(25.000,75.000)")
    ));
    QVERIFY(tripwire_feedback.log_detail.contains(
        QStringLiteral("core=north|0.5|1.25")
    ));

    const QDateTime current_time(
        QDate(2026, 3, 29), QTime(12, 0), QTimeZone::UTC
    );

    yodau::core::event motion_event;
    motion_event.kind = yodau::core::event_kind::motion;
    motion_event.stream_name = "cam-1";
    motion_event.message = "detector-hit";
    motion_event.pos_pct = yodau::core::point { 50.0f, 20.0f };

    const auto first_motion
        = feedback_state.consume_event(motion_event, current_time);
    QVERIFY(first_motion.motion_activity_changed);
    QVERIFY(first_motion.allow_gui_overlay);
    QCOMPARE(first_motion.log_severity, app_log_severity::debug);
    QCOMPARE(first_motion.log_message, QStringLiteral("motion detected"));
    QCOMPARE(feedback_state.recent_motion_count(), 1);

    const auto second_motion
        = feedback_state.consume_event(motion_event, current_time);
    QVERIFY(second_motion.motion_activity_changed);
    QVERIFY(!second_motion.allow_gui_overlay);
    QCOMPARE(feedback_state.recent_motion_count(), 2);
}

// NOLINTEND(readability-convert-member-functions-to-static)
