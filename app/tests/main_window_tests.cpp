#include "include/main_window_tests.hpp"

#include "shell/active_edit_session.hpp"
#include "shell/frontend_log.hpp"
#include "shell/frontend_settings.hpp"
#include "shell/processing_feedback_state.hpp"
#include "shell/stream_catalog_state.hpp"
#include "shell/stream_route_state.hpp"
#include "shell/stream_widget_bridge.hpp"
#include "widgets/settings_panel.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/stream_board.hpp"
#include "widgets/stream_cell.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QGroupBox>
#include <QImage>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QTabWidget>
#include <QTemporaryDir>
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

QImage render_stream_cell_event_region(const stream_settings& settings_value) {
    stream_cell cell(QStringLiteral("cam-render"));
    cell.resize(240, 180);
    cell.setStyleSheet(QStringLiteral("background-color: black; color: white;"));
    cell.set_stream_settings(settings_value);
    cell.add_event(QPointF(25.0, 25.0), QColor(QStringLiteral("#ff8844")));

    QImage image(cell.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    cell.render(&image);

    return image.copy(QRect(16, 12, 88, 68));
}

QImage render_stream_cell_wave_region(const stream_cell::line_instance& line_value) {
    stream_cell cell(QStringLiteral("cam-wave"));
    cell.resize(240, 180);
    cell.setStyleSheet(QStringLiteral("background-color: black; color: white;"));
    cell.set_persistent_lines({ line_value });
    cell.highlight_line_at(
        line_value.template_name, QPointF(50.0, 70.0), 1.0, QString(), 1.0
    );

    QImage image(cell.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    cell.render(&image);

    return image.copy(QRect(24, 98, 192, 56));
}

int differing_pixel_count(const QImage& lhs, const QImage& rhs) {
    const int width = std::min(lhs.width(), rhs.width());
    const int height = std::min(lhs.height(), rhs.height());
    int diff_count = 0;

    for (int y = 0; y < height; y += 1) {
        for (int x = 0; x < width; x += 1) {
            if (lhs.pixelColor(x, y) != rhs.pixelColor(x, y)) {
                diff_count += 1;
            }
        }
    }

    return diff_count;
}

} // namespace main_window_tests_support

void main_window_tests::active_edit_session_tracks_draft_templates_and_lines() {
    active_edit_session session;

    session.set_draft_line_profile(
        line_profile {
            .name = QStringLiteral(" north "),
            .color = QColor(Qt::green),
            .closed = true,
            .width_text = QStringLiteral("String Heavy"),
            .length_text = QStringLiteral("long"),
            .response_text = QStringLiteral("resonant"),
        }
    );

    const line_profile& draft_profile = session.draft_line_profile();
    QCOMPARE(draft_profile.name, QStringLiteral("north"));
    QCOMPARE(draft_profile.width_text, QStringLiteral("string_heavy"));
    QCOMPARE(draft_profile.length_text, QStringLiteral("long"));
    QCOMPARE(draft_profile.response_text, QStringLiteral("resonant"));

    const stream_cell::line_instance saved_line = session.store_saved_line(
        QStringLiteral("cam-1"), QStringLiteral("north"),
        {
            QPointF(10.0, 10.0),
            QPointF(90.0, 10.0),
        },
        true
    );

    QCOMPARE(saved_line.template_name, QStringLiteral("north"));
    QVERIFY(session.has_template(QStringLiteral("north")));
    QCOMPARE(
        static_cast<int>(session.stream_lines(QStringLiteral("cam-1")).size()),
        1
    );

    template_apply_settings resolved_settings
        = session.resolved_template_settings(
            template_apply_settings {
                .template_name = QStringLiteral("north"),
                .color = QColor(Qt::yellow),
                .width_text = QStringLiteral("thin"),
                .length_text = QStringLiteral("short"),
                .response_text = QStringLiteral("dry"),
            },
            true
        );
    QCOMPARE(resolved_settings.width_text, QStringLiteral("string_heavy"));
    QCOMPARE(resolved_settings.length_text, QStringLiteral("long"));
    QCOMPARE(resolved_settings.response_text, QStringLiteral("resonant"));

    session.set_active_template_settings(resolved_settings);
    const stream_cell::line_instance applied_line
        = session.store_applied_template_line(
            QStringLiteral("cam-1"), session.active_template_settings()
        );

    QCOMPARE(applied_line.template_name, QStringLiteral("north"));
    QCOMPARE(applied_line.width_text, QStringLiteral("string_heavy"));
    QCOMPARE(applied_line.length_text, QStringLiteral("long"));
    QCOMPARE(applied_line.response_text, QStringLiteral("resonant"));
    QCOMPARE(
        static_cast<int>(session.stream_lines(QStringLiteral("cam-1")).size()),
        2
    );

    const QSet<QString> used
        = session.used_template_names_for_stream(QStringLiteral("cam-1"));
    QVERIFY(used.contains(QStringLiteral("north")));
    QVERIFY(session.template_candidates_excluding(used).isEmpty());
}

void main_window_tests::
    processing_feedback_state_tracks_log_details_and_motion_throttle() {
    processing_feedback_state feedback_state;

    yodau::backend::event tripwire_event;
    tripwire_event.kind = yodau::backend::event_kind::tripwire;
    tripwire_event.stream_name = "cam-1";
    tripwire_event.line_name = "north";
    tripwire_event.message = "north|0.5|1.25";
    tripwire_event.pos_pct = yodau::backend::point { 25.0f, 75.0f };

    const auto tripwire_feedback = feedback_state.consume_event(tripwire_event);
    QCOMPARE(tripwire_feedback.kind_text, QStringLiteral("tripwire"));
    QCOMPARE(tripwire_feedback.log_message, QStringLiteral("tripwire triggered"));
    QCOMPARE(tripwire_feedback.log_severity, frontend_log_severity::info);
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
    QVERIFY(tripwire_feedback.log_detail.contains(QStringLiteral("line=north")));
    QVERIFY(
        tripwire_feedback.log_detail.contains(QStringLiteral("pos=(25.000,75.000)"))
    );
    QVERIFY(
        tripwire_feedback.log_detail.contains(
            QStringLiteral("backend=north|0.5|1.25")
        )
    );

    const QDateTime current_time(
        QDate(2026, 3, 29), QTime(12, 0), QTimeZone::UTC
    );

    yodau::backend::event motion_event;
    motion_event.kind = yodau::backend::event_kind::motion;
    motion_event.stream_name = "cam-1";
    motion_event.message = "detector-hit";
    motion_event.pos_pct = yodau::backend::point { 50.0f, 20.0f };

    const auto first_motion = feedback_state.consume_event(
        motion_event, current_time
    );
    QVERIFY(first_motion.motion_activity_changed);
    QVERIFY(first_motion.allow_gui_overlay);
    QCOMPARE(first_motion.log_severity, frontend_log_severity::debug);
    QCOMPARE(first_motion.log_message, QStringLiteral("motion detected"));
    QCOMPARE(feedback_state.recent_motion_count(), 1);

    const auto second_motion = feedback_state.consume_event(
        motion_event, current_time
    );
    QVERIFY(second_motion.motion_activity_changed);
    QVERIFY(!second_motion.allow_gui_overlay);
    QCOMPARE(feedback_state.recent_motion_count(), 2);
}

void main_window_tests::stream_catalog_state_tracks_settings_and_local_sources() {
    stream_catalog_state catalog_state;

    const stream_settings fallback_settings
        = catalog_state.settings_for(QStringLiteral(" cam-1 "));
    QCOMPARE(fallback_settings.stream_name, QStringLiteral("cam-1"));
    QVERIFY(fallback_settings.labels_enabled);
    QCOMPARE(
        fallback_settings.algorithm_id, QStringLiteral("motion_baseline")
    );
    QCOMPARE(fallback_settings.algorithm_preset, QStringLiteral("balanced"));

    catalog_state.ensure_stream(QStringLiteral(" cam-1 "));
    const stream_settings ensured_settings
        = catalog_state.settings_for(QStringLiteral("cam-1"));
    QCOMPARE(ensured_settings.stream_name, QStringLiteral("cam-1"));
    QCOMPARE(
        catalog_state.algorithm_id_for(QStringLiteral("cam-1")),
        QStringLiteral("motion_baseline")
    );

    catalog_state.set_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral(" cam-1 "),
            .labels_enabled = false,
            .algorithm_id = QStringLiteral("contour mask"),
            .algorithm_preset = QStringLiteral("debug"),
            .algorithm_overlay_enabled = true,
        }
    );

    const stream_settings saved_settings
        = catalog_state.settings_for(QStringLiteral("cam-1"));
    QCOMPARE(saved_settings.stream_name, QStringLiteral("cam-1"));
    QVERIFY(!saved_settings.labels_enabled);
    QCOMPARE(saved_settings.algorithm_id, QStringLiteral("contour_mask"));
    QCOMPARE(saved_settings.algorithm_preset, QStringLiteral("mask_heavy"));
    QVERIFY(saved_settings.algorithm_overlay_enabled);

    const QStringList locals = stream_catalog_state::detected_local_sources(
        std::vector<std::string> {
            "video0",
            " cam-1 ",
            "video2",
            "rtsp-front-door",
        }
    );
    QCOMPARE(
        locals, QStringList({ QStringLiteral("video0"), QStringLiteral("video2") })
    );
}

void main_window_tests::stream_route_state_tracks_active_stream_and_add_validation() {
    stream_route_state route_state;

    QVERIFY(!route_state.has_active_stream());
    QCOMPARE(route_state.active_stream_name(), QString());

    route_state.set_active_stream(QStringLiteral(" cam-1 "));
    QVERIFY(route_state.has_active_stream());
    QCOMPARE(route_state.active_stream_name(), QStringLiteral("cam-1"));
    QVERIFY(route_state.is_active_stream(QStringLiteral("cam-1")));
    QCOMPARE(
        route_state.next_active_stream_for_enlarge(QStringLiteral("cam-1")),
        QString()
    );
    QCOMPARE(
        route_state.next_active_stream_for_enlarge(QStringLiteral("cam-2")),
        QStringLiteral("cam-2")
    );

    QVERIFY(!route_state.hide_stream(QStringLiteral("cam-2")));
    QVERIFY(route_state.has_active_stream());
    QVERIFY(route_state.hide_stream(QStringLiteral("cam-1")));
    QVERIFY(!route_state.has_active_stream());

    const auto valid_url = stream_route_state::validate_add_source(
        QStringLiteral("https://example.test/live"), QStringLiteral("url")
    );
    QVERIFY(valid_url.valid);
    QVERIFY(valid_url.message.isEmpty());

    const auto invalid_url = stream_route_state::validate_add_source(
        QStringLiteral("camera-feed"), QStringLiteral("url")
    );
    QVERIFY(!invalid_url.valid);
    QCOMPARE(invalid_url.message, QStringLiteral("invalid url"));
    QCOMPARE(invalid_url.detail, QStringLiteral("camera-feed"));

    const auto invalid_scheme = stream_route_state::validate_add_source(
        QStringLiteral("ftp://example.test/live"), QStringLiteral("url")
    );
    QVERIFY(!invalid_scheme.valid);
    QCOMPARE(invalid_scheme.message, QStringLiteral("unsupported url scheme"));
    QCOMPARE(invalid_scheme.detail, QStringLiteral("ftp"));

    const auto local_source = stream_route_state::validate_add_source(
        QStringLiteral("/tmp/cam.mp4"), QStringLiteral("file")
    );
    QVERIFY(local_source.valid);

    QCOMPARE(
        stream_route_state::source_description(
            QStringLiteral("rtsp://cam"), QStringLiteral("url")
        ),
        QStringLiteral("url:rtsp://cam")
    );
}

void main_window_tests::
    stream_widget_bridge_applies_active_state_and_template_preview() {
    settings_panel panel;
    stream_board board;
    stream_widget_bridge widget_bridge(&board, &panel);
    stream_route_state route_state;
    active_edit_session edit_session;

    board.grid_mode()->add_stream(QStringLiteral("cam-1"));
    board.grid_mode()->add_stream(QStringLiteral("cam-2"));
    widget_bridge.sync_active_candidates();

    edit_session.set_draft_line_profile(
        line_profile {
            .name = QStringLiteral(" north "),
            .color = QColor(Qt::green),
            .closed = true,
            .width_text = QStringLiteral("string heavy"),
            .length_text = QStringLiteral("long"),
            .response_text = QStringLiteral("resonant"),
        }
    );

    edit_session.store_saved_line(
        QStringLiteral("cam-1"), QStringLiteral("north"),
        {
            QPointF(10.0, 20.0),
            QPointF(80.0, 30.0),
        },
        true
    );

    edit_session.set_drawing_new_mode(false);
    edit_session.set_active_template_settings(
        template_apply_settings {
            .template_name = QStringLiteral("north"),
            .color = QColor(Qt::yellow),
            .width_text = QStringLiteral("thin"),
            .length_text = QStringLiteral("long"),
            .response_text = QStringLiteral("resonant"),
        }
    );

    const stream_settings active_settings {
        .stream_name = QStringLiteral("cam-1"),
        .labels_enabled = false,
        .algorithm_id = QStringLiteral("contour mask"),
        .algorithm_preset = QStringLiteral("debug"),
        .algorithm_overlay_enabled = true,
    };

    route_state.set_active_stream(QStringLiteral("cam-1"));
    widget_bridge.apply_active_stream(
        route_state.active_stream_name(), active_settings, edit_session
    );
    widget_bridge.sync_active_persistent(
        route_state.active_stream_name(), edit_session
    );

    auto* active_cell = board.active_cell();
    QVERIFY(active_cell != nullptr);
    QCOMPARE(active_cell->get_name(), QStringLiteral("cam-1"));
    QCOMPARE(
        widget_bridge.tile_for_stream_name(QStringLiteral("cam-1"), route_state),
        active_cell
    );
    QCOMPARE(
        active_cell->current_stream_settings().algorithm_id,
        QStringLiteral("contour_mask")
    );
    QVERIFY(!active_cell->current_stream_settings().labels_enabled);
    QCOMPARE(active_cell->draft_name(), QStringLiteral("north"));
    QCOMPARE(active_cell->draft_color(), QColor(Qt::yellow));
    QVERIFY(active_cell->draft_closed());
    QCOMPARE(static_cast<int>(active_cell->draft_points_pct().size()), 2);
    QCOMPARE(
        panel.current_active_stream_settings().stream_name,
        QStringLiteral("cam-1")
    );
    QVERIFY(!panel.current_active_stream_settings().labels_enabled);

    const stream_settings grid_settings {
        .stream_name = QStringLiteral("cam-2"),
        .labels_enabled = true,
        .algorithm_id = QStringLiteral("spot grid"),
        .algorithm_preset = QStringLiteral("fast"),
        .algorithm_overlay_enabled = false,
    };
    widget_bridge.sync_stream_visual_settings(
        QStringLiteral("cam-2"), grid_settings, route_state
    );

    auto* grid_cell = board.grid_mode()->peek_stream_cell(QStringLiteral("cam-2"));
    QVERIFY(grid_cell != nullptr);
    QCOMPARE(
        grid_cell->current_stream_settings().algorithm_id,
        QStringLiteral("spot_grid")
    );

    widget_bridge.apply_active_stream(QString(), stream_settings {}, edit_session);
    QVERIFY(board.active_cell() == nullptr);
    QCOMPARE(panel.current_active_stream_settings().stream_name, QString());
}

void main_window_tests::frontend_settings_normalize_algorithm_and_line_width() {
    QCOMPARE(
        normalized_frontend_algorithm_id(QStringLiteral("contour mask")),
        QStringLiteral("contour_mask")
    );
    QCOMPARE(
        normalized_frontend_algorithm_id(QStringLiteral("spots")),
        QStringLiteral("spot_grid")
    );
    QCOMPARE(
        normalized_frontend_algorithm_id(QString()),
        QStringLiteral("motion_baseline")
    );
    QCOMPARE(
        normalized_algorithm_preset_id(
            QStringLiteral("contour_mask"), QStringLiteral("debug")
        ),
        QStringLiteral("mask_heavy")
    );
    QCOMPARE(
        default_algorithm_preset_id(QStringLiteral("spot_grid")),
        QStringLiteral("balanced")
    );
    QVERIFY(
        algorithm_summary_text(
            QStringLiteral("spot_grid"), QStringLiteral("dense"), true
        )
            .contains(QStringLiteral("point-style motion regions"))
    );

    QCOMPARE(
        normalized_line_width_text(QStringLiteral("String Heavy")),
        QStringLiteral("string_heavy")
    );
    QCOMPARE(
        normalized_line_length_text(QStringLiteral("drone")),
        QStringLiteral("long")
    );
    QCOMPARE(
        normalized_line_response_text(QStringLiteral("ringing")),
        QStringLiteral("resonant")
    );
    QCOMPARE(
        normalized_line_width_text(QStringLiteral("6")),
        QStringLiteral("6.0")
    );
    QCOMPARE(line_width_visual_value(QStringLiteral("thin")), 2.0);
    QCOMPARE(line_width_visual_value(QStringLiteral("string_heavy")), 6.5);
    QCOMPARE(
        line_profile_summary_text(
            QStringLiteral("String Heavy"), QStringLiteral("long"),
            QStringLiteral("ringing")
        ),
        QStringLiteral("width=string_heavy length=long response=resonant")
    );
}

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

void main_window_tests::settings_panel_round_trips_structured_settings() {
    settings_panel panel;
    panel.set_active_candidates({ QStringLiteral("cam-1"), QStringLiteral("cam-2") });
    panel.set_template_candidates({ QStringLiteral("north"), QStringLiteral("south") });

    panel.set_active_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-2"),
            .labels_enabled = false,
            .algorithm_id = QStringLiteral("contour mask"),
            .algorithm_preset = QStringLiteral("mask_heavy"),
            .algorithm_overlay_enabled = true,
        }
    );

    const stream_settings active_stream = panel.current_active_stream_settings();
    QCOMPARE(active_stream.stream_name, QStringLiteral("cam-2"));
    QCOMPARE(active_stream.labels_enabled, false);
    QCOMPARE(active_stream.algorithm_id, QStringLiteral("contour_mask"));
    QCOMPARE(active_stream.algorithm_preset, QStringLiteral("mask_heavy"));
    QCOMPARE(active_stream.algorithm_overlay_enabled, true);

    panel.set_active_line_profile(
        line_profile {
            .name = QStringLiteral("north"),
            .color = QColor(Qt::green),
            .closed = true,
            .width_text = QStringLiteral("String Heavy"),
            .length_text = QStringLiteral("long"),
            .response_text = QStringLiteral("resonant"),
        }
    );

    const line_profile active_line = panel.current_active_line_profile();
    QCOMPARE(active_line.name, QStringLiteral("north"));
    QCOMPARE(active_line.color, QColor(Qt::green));
    QCOMPARE(active_line.closed, true);
    QCOMPARE(active_line.width_text, QStringLiteral("string_heavy"));
    QCOMPARE(active_line.length_text, QStringLiteral("long"));
    QCOMPARE(active_line.response_text, QStringLiteral("resonant"));

    panel.set_active_template_settings(
        template_apply_settings {
            .template_name = QStringLiteral("south"),
            .color = QColor(Qt::yellow),
            .width_text = QStringLiteral("thin"),
            .length_text = QStringLiteral("short"),
            .response_text = QStringLiteral("dry"),
        }
    );

    const template_apply_settings active_template
        = panel.current_active_template_settings();
    QCOMPARE(active_template.template_name, QStringLiteral("south"));
    QCOMPARE(active_template.color, QColor(Qt::yellow));
    QCOMPARE(active_template.width_text, QStringLiteral("thin"));
    QCOMPARE(active_template.length_text, QStringLiteral("short"));
    QCOMPARE(active_template.response_text, QStringLiteral("dry"));
}

void main_window_tests::settings_panel_exposes_explicit_edit_panels() {
    settings_panel panel;
    panel.set_active_candidates({ QStringLiteral("cam-1") });
    panel.set_template_candidates({ QStringLiteral("north") });
    panel.set_active_current(QStringLiteral("cam-1"));

    auto* line_panel = panel.findChild<QGroupBox*>(
        QStringLiteral("settings_active_line_profile_panel")
    );
    auto* template_panel = panel.findChild<QGroupBox*>(
        QStringLiteral("settings_active_template_apply_panel")
    );
    auto* line_summary = panel.findChild<QLabel*>(
        QStringLiteral("settings_active_line_summary_label")
    );
    auto* template_summary = panel.findChild<QLabel*>(
        QStringLiteral("settings_active_template_summary_label")
    );

    QVERIFY(line_panel != nullptr);
    QVERIFY(template_panel != nullptr);
    QVERIFY(line_summary != nullptr);
    QVERIFY(template_summary != nullptr);

    panel.set_active_line_profile(
        line_profile {
            .name = QStringLiteral("north"),
            .color = QColor(Qt::cyan),
            .closed = false,
            .width_text = QStringLiteral("string_light"),
            .length_text = QStringLiteral("long"),
            .response_text = QStringLiteral("resonant"),
        }
    );
    panel.set_active_template_settings(
        template_apply_settings {
            .template_name = QStringLiteral("north"),
            .color = QColor(Qt::magenta),
            .width_text = QStringLiteral("thick"),
            .length_text = QStringLiteral("short"),
            .response_text = QStringLiteral("dry"),
        }
    );

    QVERIFY(line_summary->text().contains(QStringLiteral("width=string_light")));
    QVERIFY(line_summary->text().contains(QStringLiteral("length=long")));
    QVERIFY(line_summary->text().contains(QStringLiteral("response=resonant")));

    QVERIFY(template_summary->text().contains(QStringLiteral("north")));
    QVERIFY(template_summary->text().contains(QStringLiteral("width=thick")));
    QVERIFY(template_summary->text().contains(QStringLiteral("length=short")));
    QVERIFY(template_summary->text().contains(QStringLiteral("response=dry")));
}

void main_window_tests::settings_panel_emits_structured_stream_settings() {
    settings_panel panel;
    panel.set_active_candidates({ QStringLiteral("cam-1") });

    QSignalSpy stream_settings_spy(
        &panel, &settings_panel::active_stream_settings_changed
    );

    auto* active_stream_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_stream_combo")
    );
    auto* active_algorithm_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_algorithm_combo")
    );
    auto* active_labels_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_active_labels_checkbox")
    );

    QVERIFY(active_stream_combo != nullptr);
    QVERIFY(active_algorithm_combo != nullptr);
    QVERIFY(active_labels_checkbox != nullptr);

    active_stream_combo->setCurrentText(QStringLiteral("cam-1"));

    QCOMPARE(stream_settings_spy.count(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        QVERIFY(args.size() == 1);
        const stream_settings settings_value
            = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(settings_value.labels_enabled, true);
        QCOMPARE(
            settings_value.algorithm_id, QStringLiteral("motion_baseline")
        );
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("balanced"));
        QCOMPARE(settings_value.algorithm_overlay_enabled, false);
    }

    const int contour_mask_index
        = active_algorithm_combo->findData(QStringLiteral("contour_mask"));
    QVERIFY(contour_mask_index >= 0);
    active_algorithm_combo->setCurrentIndex(contour_mask_index);

    QCOMPARE(stream_settings_spy.count(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const stream_settings settings_value
            = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(
            settings_value.algorithm_id, QStringLiteral("contour_mask")
        );
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("outline"));
    }

    active_labels_checkbox->setChecked(false);

    QCOMPARE(stream_settings_spy.count(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const stream_settings settings_value
            = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(settings_value.labels_enabled, false);
        QCOMPARE(
            settings_value.algorithm_id, QStringLiteral("contour_mask")
        );
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("outline"));
    }
}

void main_window_tests::settings_panel_updates_algorithm_presets_by_selection() {
    settings_panel panel;
    panel.set_active_candidates({ QStringLiteral("cam-1") });
    panel.set_active_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-1"),
            .labels_enabled = true,
            .algorithm_id = QStringLiteral("motion_baseline"),
            .algorithm_preset = QStringLiteral("balanced"),
            .algorithm_overlay_enabled = false,
        }
    );

    QSignalSpy stream_settings_spy(
        &panel, &settings_panel::active_stream_settings_changed
    );

    auto* algorithm_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_algorithm_combo")
    );
    auto* preset_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_algorithm_preset_combo")
    );
    auto* overlay_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_active_algorithm_overlay_checkbox")
    );
    auto* summary_label = panel.findChild<QLabel*>(
        QStringLiteral("settings_active_algorithm_summary_label")
    );

    QVERIFY(algorithm_combo != nullptr);
    QVERIFY(preset_combo != nullptr);
    QVERIFY(overlay_checkbox != nullptr);
    QVERIFY(summary_label != nullptr);

    const int contour_mask_index
        = algorithm_combo->findData(QStringLiteral("contour_mask"));
    QVERIFY(contour_mask_index >= 0);
    algorithm_combo->setCurrentIndex(contour_mask_index);

    QCOMPARE(stream_settings_spy.count(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const stream_settings settings_value
            = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(
            settings_value.algorithm_id, QStringLiteral("contour_mask")
        );
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("outline"));
        QCOMPARE(settings_value.algorithm_overlay_enabled, false);
    }

    QCOMPARE(
        preset_combo->itemData(0).toString(), QStringLiteral("outline")
    );
    QCOMPARE(
        preset_combo->itemData(2).toString(), QStringLiteral("mask_heavy")
    );
    QVERIFY(summary_label->text().contains(QStringLiteral("contours and masks")));

    overlay_checkbox->setChecked(true);
    QCOMPARE(stream_settings_spy.count(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const stream_settings settings_value
            = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(
            settings_value.algorithm_id, QStringLiteral("contour_mask")
        );
        QCOMPARE(settings_value.algorithm_overlay_enabled, true);
    }

    const int mask_heavy_index
        = preset_combo->findData(QStringLiteral("mask_heavy"));
    QVERIFY(mask_heavy_index >= 0);
    preset_combo->setCurrentIndex(mask_heavy_index);

    QCOMPARE(stream_settings_spy.count(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const stream_settings settings_value
            = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("mask_heavy"));
        QCOMPARE(settings_value.algorithm_overlay_enabled, true);
    }
}

void main_window_tests::settings_panel_exports_current_filtered_log_report() {
    settings_panel panel;
    frontend_log_buffer buffer;

    panel.set_log_buffer(&buffer);

    auto* log_mode_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_log_mode_combo")
    );
    auto* tabs = panel.findChild<QTabWidget*>();

    QVERIFY(log_mode_combo != nullptr);
    QVERIFY(tabs != nullptr);

    const QDateTime timestamp(
        QDate(2026, 3, 29), QTime(14, 15, 16, 17), QTimeZone::UTC
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, frontend_log_area::active, frontend_log_severity::debug,
            QStringLiteral("backend_event"),
            QStringLiteral("motion detected"), QStringLiteral("cam-1"),
            QStringLiteral("cells=9"), QStringLiteral("spot_grid")
        )
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, frontend_log_area::active, frontend_log_severity::warning,
            QStringLiteral("stream_settings"),
            QStringLiteral("algorithm preference updated"),
            QStringLiteral("cam-1"),
            QStringLiteral("backend runtime still uses baseline processing"),
            QStringLiteral("contour_mask")
        )
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, frontend_log_area::streams,
            frontend_log_severity::info, QStringLiteral("grid_visibility"),
            QStringLiteral("stream shown in grid"), QStringLiteral("cam-2")
        )
    );

    tabs->setCurrentIndex(2);
    log_mode_combo->setCurrentIndex(1);

    const QString report = panel.compose_current_log_report();
    QVERIFY(report.contains(QStringLiteral("yodau log report")));
    QVERIFY(report.contains(QStringLiteral("area=active mode=debug")));
    QVERIFY(report.contains(QStringLiteral("motion detected")));
    QVERIFY(report.contains(QStringLiteral("algorithm preference updated")));
    QVERIFY(!report.contains(QStringLiteral("stream shown in grid")));

    const QString summary = panel.compose_current_log_summary();
    QVERIFY(summary.contains(QStringLiteral("yodau log summary")));
    QVERIFY(summary.contains(QStringLiteral("area=active")));
    QVERIFY(summary.contains(QStringLiteral("entries=2")));
    QVERIFY(summary.contains(QStringLiteral("debug=1")));
    QVERIFY(summary.contains(QStringLiteral("warn=1")));
    QVERIFY(summary.contains(QStringLiteral("stream_list=cam-1")));

    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());

    const QString output_path = temp_dir.filePath(
        QStringLiteral("report.txt")
    );
    QVERIFY(panel.write_current_log_report(output_path));

    QFile report_file(output_path);
    QVERIFY(report_file.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString::fromUtf8(report_file.readAll()), report);
}

void main_window_tests::stream_cell_tracks_stream_settings() {
    stream_cell cell(QStringLiteral("cam-7"));

    cell.set_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-7"),
            .labels_enabled = true,
            .algorithm_id = QStringLiteral("contour"),
            .algorithm_preset = QStringLiteral("debug"),
            .algorithm_overlay_enabled = true,
        }
    );

    const stream_settings settings_value = cell.current_stream_settings();
    QCOMPARE(settings_value.stream_name, QStringLiteral("cam-7"));
    QCOMPARE(settings_value.algorithm_id, QStringLiteral("contour_mask"));
    QCOMPARE(settings_value.algorithm_preset, QStringLiteral("mask_heavy"));
    QCOMPARE(settings_value.algorithm_overlay_enabled, true);
}

void main_window_tests::stream_cell_overlay_modes_render_different_event_regions() {
    const stream_settings no_overlay {
        .stream_name = QStringLiteral("cam-render"),
        .labels_enabled = true,
        .algorithm_id = QStringLiteral("motion_baseline"),
        .algorithm_preset = QStringLiteral("balanced"),
        .algorithm_overlay_enabled = false,
    };
    const stream_settings baseline_overlay {
        .stream_name = QStringLiteral("cam-render"),
        .labels_enabled = true,
        .algorithm_id = QStringLiteral("motion_baseline"),
        .algorithm_preset = QStringLiteral("debug"),
        .algorithm_overlay_enabled = true,
    };
    const stream_settings spot_overlay {
        .stream_name = QStringLiteral("cam-render"),
        .labels_enabled = true,
        .algorithm_id = QStringLiteral("spot_grid"),
        .algorithm_preset = QStringLiteral("dense"),
        .algorithm_overlay_enabled = true,
    };
    const stream_settings contour_overlay {
        .stream_name = QStringLiteral("cam-render"),
        .labels_enabled = true,
        .algorithm_id = QStringLiteral("contour_mask"),
        .algorithm_preset = QStringLiteral("mask_heavy"),
        .algorithm_overlay_enabled = true,
    };

    const QImage no_overlay_image
        = main_window_tests_support::render_stream_cell_event_region(no_overlay);
    const QImage baseline_overlay_image
        = main_window_tests_support::render_stream_cell_event_region(
            baseline_overlay
        );
    const QImage spot_overlay_image
        = main_window_tests_support::render_stream_cell_event_region(
            spot_overlay
        );
    const QImage contour_overlay_image
        = main_window_tests_support::render_stream_cell_event_region(
            contour_overlay
        );

    QVERIFY(
        main_window_tests_support::differing_pixel_count(
            no_overlay_image, baseline_overlay_image
        )
            > 80
    );
    QVERIFY(
        main_window_tests_support::differing_pixel_count(
            baseline_overlay_image, spot_overlay_image
        )
            > 120
    );
    QVERIFY(
        main_window_tests_support::differing_pixel_count(
            spot_overlay_image, contour_overlay_image
        )
            > 120
    );
}

void main_window_tests::stream_cell_line_profiles_render_different_wave_regions() {
    stream_cell::line_instance dry_profile;
    dry_profile.template_name = QStringLiteral("line-a");
    dry_profile.color = QColor(QStringLiteral("#67c1ff"));
    dry_profile.width_text = QStringLiteral("thin");
    dry_profile.length_text = QStringLiteral("short");
    dry_profile.response_text = QStringLiteral("dry");
    dry_profile.pts_pct = {
        QPointF(15.0, 70.0),
        QPointF(50.0, 70.0),
        QPointF(85.0, 70.0),
    };

    stream_cell::line_instance resonant_profile = dry_profile;
    resonant_profile.width_text = QStringLiteral("string_heavy");
    resonant_profile.length_text = QStringLiteral("long");
    resonant_profile.response_text = QStringLiteral("resonant");

    const QImage dry_region
        = main_window_tests_support::render_stream_cell_wave_region(
            dry_profile
        );
    const QImage resonant_region
        = main_window_tests_support::render_stream_cell_wave_region(
            resonant_profile
        );

    QVERIFY(
        main_window_tests_support::differing_pixel_count(
            dry_region, resonant_region
        )
            > 500
    );
}
