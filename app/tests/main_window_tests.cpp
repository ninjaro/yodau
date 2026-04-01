#include "include/main_window_tests.hpp"

#include "shell/active_edit_actions.hpp"
#include "shell/active_edit_controller.hpp"
#include "shell/active_edit_workflow.hpp"
#include "shell/active_editor_bridge.hpp"
#include "shell/active_edit_session.hpp"
#include "shell/active_stream_state.hpp"
#include "shell/active_stream_workflow.hpp"
#include "shell/frontend_log.hpp"
#include "shell/frontend_settings.hpp"
#include "shell/processing_feedback_state.hpp"
#include "shell/stream_catalog_state.hpp"
#include "shell/stream_catalog_workflow.hpp"
#include "shell/stream_route_state.hpp"
#include "shell/stream_widget_bridge.hpp"
#include "streams/stream_manager.hpp"
#include "widgets/active_editor_panel.hpp"
#include "widgets/active_stream_panel.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/log_area_view.hpp"
#include "widgets/log_toolbar_panel.hpp"
#include "widgets/settings_panel.hpp"
#include "widgets/stream_board.hpp"
#include "widgets/stream_cell.hpp"
#include "widgets/stream_inventory_panel.hpp"
#include "widgets/stream_source_panel.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QGroupBox>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QTimeZone>
#include <QtTest/QtTest>

#include <cmath>

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

QImage render_stream_cell_status_region(
    const stream_settings& settings_value, const frontend_log_mode log_mode
) {
    stream_cell cell(QStringLiteral("cam-status"));
    cell.resize(240, 180);
    cell.setStyleSheet(QStringLiteral("background-color: black; color: white;"));
    cell.set_stream_settings(settings_value);
    cell.set_log_mode(log_mode);

    QImage image(cell.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    cell.render(&image);

    return image.copy(QRect(96, 118, 136, 54));
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

void main_window_tests::active_edit_actions_apply_lines_and_templates_against_backend() {
    settings_panel panel;
    stream_board board;
    stream_widget_bridge widget_bridge(&board, &panel);
    active_edit_session edit_session;
    active_edit_controller edit_controller(edit_session, widget_bridge);
    yodau::backend::stream_manager stream_mgr;
    active_edit_actions edit_actions(
        &stream_mgr, edit_session, widget_bridge, edit_controller
    );

    stream_mgr.add_stream("/tmp/cam-1.mp4", "cam-1", "file", true);

    const stream_settings active_settings {
        .stream_name = QStringLiteral("cam-1"),
        .labels_enabled = true,
        .algorithm_id = QStringLiteral("motion_baseline"),
        .algorithm_preset = QStringLiteral("balanced"),
        .algorithm_overlay_enabled = false,
    };
    auto* grid_tile = widget_bridge.show_stream_in_grid(
        QStringLiteral("cam-1"), active_settings, edit_session,
        stream_widget_bridge::grid_stream_binding {
            .path = QStringLiteral("/tmp/cam-1.mp4"),
            .type = QStringLiteral("file"),
            .loop = true,
        }
    );
    QVERIFY(grid_tile != nullptr);
    widget_bridge.apply_active_stream(QStringLiteral("cam-1"), active_settings, edit_session);
    widget_bridge.sync_active_persistent(QStringLiteral("cam-1"), edit_session);

    auto* active_cell = board.active_cell();
    QVERIFY(active_cell != nullptr);
    active_cell->set_draft_points_pct(
        {
            QPointF(10.0, 10.0),
            QPointF(80.0, 20.0),
            QPointF(60.0, 75.0),
        }
    );

    const auto line_result = edit_actions.save_active_line(
        QStringLiteral("cam-1"),
        line_profile {
            .name = QStringLiteral(" north "),
            .color = QColor(Qt::cyan),
            .closed = true,
            .width_text = QStringLiteral("string_heavy"),
            .length_text = QStringLiteral("long"),
            .response_text = QStringLiteral("resonant"),
        },
        *active_cell
    );

    QCOMPARE(line_result.status, active_edit_actions::line_save_status::saved);
    QCOMPARE(line_result.profile.name, QStringLiteral("north"));
    QCOMPARE(line_result.final_name, QStringLiteral("north"));
    QCOMPARE(line_result.point_count, 3);
    QVERIFY(line_result.points_text.contains(QStringLiteral("(10.000,10.000)")));
    QCOMPARE(static_cast<int>(stream_mgr.stream_lines("cam-1").size()), 1);
    QCOMPARE(
        QString::fromStdString(stream_mgr.stream_lines("cam-1").front()),
        QStringLiteral("north")
    );
    QCOMPARE(static_cast<int>(edit_session.stream_lines(QStringLiteral("cam-1")).size()), 1);
    QCOMPARE(
        edit_session.stream_lines(QStringLiteral("cam-1")).front().template_name,
        QStringLiteral("north")
    );
    const auto stored_backend_profile = stream_mgr.find_line_profile("north");
    QVERIFY(stored_backend_profile.has_value());
    QVERIFY(std::fabs(stored_backend_profile->visual_width - 6.5f) < 0.01f);
    QVERIFY(
        std::fabs(stored_backend_profile->interaction_width - 8.45f) < 0.01f
    );
    QVERIFY(
        std::fabs(stored_backend_profile->effective_length - 1.35f) < 0.01f
    );
    QVERIFY(std::fabs(stored_backend_profile->damping - 0.8f) < 0.01f);

    const auto attached_backend_profile
        = stream_mgr.find_stream_line_profile("cam-1", "north");
    QVERIFY(attached_backend_profile.has_value());
    QVERIFY(
        std::fabs(
            attached_backend_profile->visual_width
            - stored_backend_profile->visual_width
        )
        < 0.01f
    );
    QCOMPARE(static_cast<int>(active_cell->draft_points_pct().size()), 0);
    QCOMPARE(panel.current_active_line_profile().name, QString());

    const auto missing_template_result = edit_actions.apply_active_template(
        QStringLiteral("cam-1"),
        template_apply_settings {
            .template_name = QStringLiteral(" missing "),
            .color = QColor(Qt::yellow),
            .width_text = QStringLiteral("thin"),
            .length_text = QStringLiteral("short"),
            .response_text = QStringLiteral("dry"),
        },
        *active_cell
    );

    QCOMPARE(
        missing_template_result.status,
        active_edit_actions::template_apply_status::unknown_template
    );
    QCOMPARE(
        missing_template_result.settings.template_name,
        QStringLiteral("missing")
    );

    const auto template_result = edit_actions.apply_active_template(
        QStringLiteral("cam-1"),
        template_apply_settings {
            .template_name = QStringLiteral(" north "),
            .color = QColor(Qt::yellow),
            .width_text = QStringLiteral("thin"),
            .length_text = QStringLiteral("short"),
            .response_text = QStringLiteral("dry"),
        },
        *active_cell
    );

    QCOMPARE(
        template_result.status,
        active_edit_actions::template_apply_status::applied
    );
    QCOMPARE(template_result.settings.template_name, QStringLiteral("north"));
    QCOMPARE(template_result.line.template_name, QStringLiteral("north"));
    QCOMPARE(template_result.line.color, QColor(Qt::yellow));
    QCOMPARE(static_cast<int>(stream_mgr.stream_lines("cam-1").size()), 1);
    QCOMPARE(static_cast<int>(edit_session.stream_lines(QStringLiteral("cam-1")).size()), 2);
    QCOMPARE(panel.current_active_template_settings().template_name, QString());
    const auto global_profile_after_apply = stream_mgr.find_line_profile("north");
    QVERIFY(global_profile_after_apply.has_value());
    QVERIFY(
        std::fabs(
            global_profile_after_apply->visual_width
            - stored_backend_profile->visual_width
        )
        < 0.01f
    );
    const auto stream_profile_after_apply
        = stream_mgr.find_stream_line_profile("cam-1", "north");
    QVERIFY(stream_profile_after_apply.has_value());
    QVERIFY(std::fabs(stream_profile_after_apply->visual_width - 2.0f) < 0.01f);
    QVERIFY(
        std::fabs(stream_profile_after_apply->interaction_width - 2.0f) < 0.01f
    );
    QVERIFY(
        std::fabs(stream_profile_after_apply->effective_length - 0.75f) < 0.01f
    );
    QVERIFY(std::fabs(stream_profile_after_apply->damping - 0.25f) < 0.01f);

    active_cell->set_draft_points_pct(
        {
            QPointF(11.0, 12.0),
            QPointF(40.0, 42.0),
        }
    );
    const auto before_line_count = static_cast<int>(
        edit_session.stream_lines(QStringLiteral("cam-1")).size()
    );
    const auto failed_line_result = edit_actions.save_active_line(
        QStringLiteral("missing-stream"),
        line_profile {
            .name = QStringLiteral("orphan"),
            .color = QColor(Qt::red),
            .closed = false,
            .width_text = QStringLiteral("medium"),
            .length_text = QStringLiteral("medium"),
            .response_text = QStringLiteral("balanced"),
        },
        *active_cell
    );

    QCOMPARE(
        failed_line_result.status,
        active_edit_actions::line_save_status::backend_error
    );
    QCOMPARE(
        static_cast<int>(edit_session.stream_lines(QStringLiteral("cam-1")).size()),
        before_line_count
    );
    QCOMPARE(
        static_cast<int>(edit_session.stream_lines(QStringLiteral("missing-stream")).size()),
        0
    );
    QCOMPARE(static_cast<int>(active_cell->draft_points_pct().size()), 2);
}

void main_window_tests::active_edit_workflow_tracks_logs_and_follow_up_actions() {
    settings_panel panel;
    stream_board board;
    stream_widget_bridge widget_bridge(&board, &panel);
    active_edit_session edit_session;
    stream_catalog_state catalog_state;
    stream_route_state route_state;
    active_edit_controller edit_controller(edit_session, widget_bridge);
    yodau::backend::stream_manager stream_mgr;
    active_edit_actions edit_actions(
        &stream_mgr, edit_session, widget_bridge, edit_controller
    );
    active_edit_workflow workflow(
        route_state, catalog_state, widget_bridge, edit_controller, edit_actions
    );

    const auto missing_active_result = workflow.save_active_line(
        line_profile {
            .name = QStringLiteral("orphan"),
            .color = QColor(Qt::red),
            .closed = false,
            .width_text = QStringLiteral("medium"),
            .length_text = QStringLiteral("medium"),
            .response_text = QStringLiteral("balanced"),
        }
    );

    QCOMPARE(missing_active_result.entries.size(), 1);
    QCOMPARE(
        missing_active_result.entries.front().message,
        QStringLiteral("add line failed: no active stream")
    );
    QCOMPARE(
        missing_active_result.entries.front().subsystem,
        QStringLiteral("active_stream")
    );
    QVERIFY(!missing_active_result.refresh_fps);

    stream_mgr.add_stream("/tmp/cam-1.mp4", "cam-1", "file", true);
    catalog_state.ensure_stream(QStringLiteral("cam-1"));
    catalog_state.set_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-1"),
            .labels_enabled = true,
            .algorithm_id = QStringLiteral("spot grid"),
            .algorithm_preset = QStringLiteral("dense"),
            .algorithm_overlay_enabled = true,
        }
    );
    route_state.set_active_stream(QStringLiteral("cam-1"));

    const stream_settings active_settings
        = catalog_state.settings_for(QStringLiteral("cam-1"));
    auto* grid_tile = widget_bridge.show_stream_in_grid(
        QStringLiteral("cam-1"), active_settings, edit_session,
        stream_widget_bridge::grid_stream_binding {
            .path = QStringLiteral("/tmp/cam-1.mp4"),
            .type = QStringLiteral("file"),
            .loop = true,
        }
    );
    QVERIFY(grid_tile != nullptr);
    widget_bridge.apply_active_stream(QStringLiteral("cam-1"), active_settings, edit_session);
    widget_bridge.sync_active_persistent(QStringLiteral("cam-1"), edit_session);

    auto* active_cell = board.active_cell();
    QVERIFY(active_cell != nullptr);

    const auto mode_result = workflow.set_drawing_new_mode(false);
    QCOMPARE(mode_result.entries.size(), 1);
    QCOMPARE(
        mode_result.entries.front().message,
        QStringLiteral("edit mode set to use template")
    );
    QCOMPARE(
        mode_result.entries.front().algorithm_id,
        QStringLiteral("spot_grid")
    );

    const auto line_profile_result = workflow.apply_line_profile(
        line_profile {
            .name = QStringLiteral(" north "),
            .color = QColor(Qt::cyan),
            .closed = true,
            .width_text = QStringLiteral("string_heavy"),
            .length_text = QStringLiteral("long"),
            .response_text = QStringLiteral("resonant"),
        }
    );

    QCOMPARE(line_profile_result.entries.size(), 1);
    QCOMPARE(
        line_profile_result.entries.front().message,
        QStringLiteral("active line draft updated")
    );
    QVERIFY(
        line_profile_result.entries.front().detail.contains(
            QStringLiteral("name=north")
        )
    );
    QCOMPARE(
        line_profile_result.entries.front().algorithm_id,
        QStringLiteral("spot_grid")
    );

    active_cell->set_draft_points_pct(
        {
            QPointF(10.0, 10.0),
            QPointF(80.0, 20.0),
            QPointF(60.0, 75.0),
        }
    );
    const auto line_save_result = workflow.save_active_line(
        line_profile {
            .name = QStringLiteral(" north "),
            .color = QColor(Qt::cyan),
            .closed = true,
            .width_text = QStringLiteral("string_heavy"),
            .length_text = QStringLiteral("long"),
            .response_text = QStringLiteral("resonant"),
        }
    );

    QCOMPARE(line_save_result.entries.size(), 3);
    QCOMPARE(
        line_save_result.entries.front().message,
        QStringLiteral("line save requested")
    );
    QCOMPARE(
        line_save_result.entries.at(1).message,
        QStringLiteral("line draft points prepared")
    );
    QCOMPARE(
        line_save_result.entries.back().message,
        QStringLiteral("line added")
    );
    QVERIFY(line_save_result.refresh_fps);
    QVERIFY(line_save_result.update_monitor_inventory);
    QCOMPARE(line_save_result.monitor_marker, QStringLiteral("line_added"));
    QVERIFY(
        line_save_result.entries.back().detail.contains(
            QStringLiteral("template=north")
        )
    );
    QCOMPARE(
        line_save_result.entries.back().algorithm_id,
        QStringLiteral("spot_grid")
    );

    const auto template_settings_result = workflow.apply_template_settings(
        template_apply_settings {
            .template_name = QStringLiteral(" north "),
            .color = QColor(Qt::yellow),
            .width_text = QStringLiteral("thin"),
            .length_text = QStringLiteral("short"),
            .response_text = QStringLiteral("dry"),
        }
    );

    QCOMPARE(template_settings_result.entries.size(), 1);
    QCOMPARE(
        template_settings_result.entries.front().message,
        QStringLiteral("template preview updated")
    );
    QVERIFY(
        template_settings_result.entries.front().detail.contains(
            QStringLiteral("template=north")
        )
    );

    const auto template_apply_result = workflow.apply_active_template(
        template_apply_settings {
            .template_name = QStringLiteral(" north "),
            .color = QColor(Qt::yellow),
            .width_text = QStringLiteral("thin"),
            .length_text = QStringLiteral("short"),
            .response_text = QStringLiteral("dry"),
        }
    );

    QCOMPARE(template_apply_result.entries.size(), 1);
    QCOMPARE(
        template_apply_result.entries.front().message,
        QStringLiteral("template added to active stream")
    );
    QVERIFY(template_apply_result.refresh_fps);
    QVERIFY(!template_apply_result.update_monitor_inventory);
    QVERIFY(template_apply_result.monitor_marker.isEmpty());
    QCOMPARE(
        template_apply_result.entries.front().algorithm_id,
        QStringLiteral("spot_grid")
    );
}

void main_window_tests::active_editor_bridge_tracks_editor_projection_and_preview() {
    settings_panel panel;
    stream_board board;
    active_editor_bridge editor_bridge(&board, &panel);
    active_edit_session edit_session;

    board.grid_mode()->add_stream(QStringLiteral("cam-1"));
    board.grid_mode()->add_stream(QStringLiteral("cam-2"));

    edit_session.set_draft_line_profile(
        line_profile {
            .name = QStringLiteral(" north "),
            .color = QColor(Qt::green),
            .closed = true,
            .width_text = QStringLiteral("string_heavy"),
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
            .length_text = QStringLiteral("short"),
            .response_text = QStringLiteral("dry"),
        }
    );

    editor_bridge.sync_active_candidates();
    editor_bridge.add_template_candidate(QStringLiteral("north"));
    editor_bridge.initialize_editor_state(edit_session);
    QCOMPARE(panel.current_active_line_profile().name, QStringLiteral("north"));
    QCOMPARE(
        panel.current_active_template_settings().template_name,
        QStringLiteral("north")
    );

    editor_bridge.apply_active_stream(
        QStringLiteral("cam-1"),
        stream_settings {
            .stream_name = QStringLiteral("cam-1"),
            .labels_enabled = false,
            .algorithm_id = QStringLiteral("contour mask"),
            .algorithm_preset = QStringLiteral("mask_heavy"),
            .algorithm_overlay_enabled = true,
        },
        edit_session
    );
    editor_bridge.sync_active_persistent(QStringLiteral("cam-1"), edit_session);

    QVERIFY(board.active_cell() != nullptr);
    QCOMPARE(board.active_cell()->get_name(), QStringLiteral("cam-1"));
    QCOMPARE(board.active_cell()->draft_name(), QStringLiteral("north"));
    QCOMPARE(board.active_cell()->draft_color(), QColor(Qt::yellow));
    QCOMPARE(static_cast<int>(board.active_cell()->draft_points_pct().size()), 2);
    QCOMPARE(
        panel.current_active_stream_settings().stream_name,
        QStringLiteral("cam-1")
    );
    QVERIFY(!panel.current_active_stream_settings().labels_enabled);

    auto* template_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_template_combo")
    );
    QVERIFY(template_combo != nullptr);
    QVERIFY(template_combo->findText(QStringLiteral("north")) < 0);

    editor_bridge.add_template_candidate(QStringLiteral("west"));
    QVERIFY(template_combo->findText(QStringLiteral("west")) >= 0);

    edit_session.reset_draft_line_profile();
    editor_bridge.sync_active_line_editor(edit_session, true);
    QCOMPARE(panel.current_active_line_profile().name, QString());

    edit_session.reset_active_template_settings();
    editor_bridge.sync_active_template_editor(edit_session, true);
    QCOMPARE(
        panel.current_active_template_settings().template_name, QString()
    );

    editor_bridge.apply_active_stream(QString(), stream_settings {}, edit_session);
    QVERIFY(board.active_cell() == nullptr);
    QCOMPARE(panel.current_active_stream_settings().stream_name, QString());
}

void main_window_tests::active_stream_state_tracks_selection_and_settings_application() {
    settings_panel panel;
    stream_board board;
    stream_widget_bridge widget_bridge(&board, &panel);
    active_edit_session edit_session;
    stream_catalog_state catalog_state;
    stream_route_state route_state;
    active_stream_state active_streams(
        catalog_state, route_state, widget_bridge, edit_session
    );

    catalog_state.ensure_stream(QStringLiteral("cam-1"));
    catalog_state.ensure_stream(QStringLiteral("cam-2"));

    auto* cam1_tile = widget_bridge.show_stream_in_grid(
        QStringLiteral("cam-1"),
        catalog_state.settings_for(QStringLiteral("cam-1")), edit_session,
        stream_widget_bridge::grid_stream_binding {}
    );
    auto* cam2_tile = widget_bridge.show_stream_in_grid(
        QStringLiteral("cam-2"),
        catalog_state.settings_for(QStringLiteral("cam-2")), edit_session,
        stream_widget_bridge::grid_stream_binding {}
    );
    QVERIFY(cam1_tile != nullptr);
    QVERIFY(cam2_tile != nullptr);

    const auto switched_to_cam2 = active_streams.apply_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral(" cam-2 "),
            .labels_enabled = true,
            .algorithm_id = QStringLiteral("motion_baseline"),
            .algorithm_preset = QStringLiteral("balanced"),
            .algorithm_overlay_enabled = false,
        }
    );

    QCOMPARE(
        switched_to_cam2.outcome_value,
        active_stream_state::settings_result::outcome::switched_active_stream
    );
    QCOMPARE(switched_to_cam2.active_name, QStringLiteral("cam-2"));
    QCOMPARE(route_state.active_stream_name(), QStringLiteral("cam-2"));
    QVERIFY(board.active_cell() != nullptr);
    QCOMPARE(board.active_cell()->get_name(), QStringLiteral("cam-2"));
    QCOMPARE(
        panel.current_active_stream_settings().stream_name,
        QStringLiteral("cam-2")
    );

    const auto updated_cam2 = active_streams.apply_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-2"),
            .labels_enabled = false,
            .algorithm_id = QStringLiteral("contour mask"),
            .algorithm_preset = QStringLiteral("mask_heavy"),
            .algorithm_overlay_enabled = true,
        }
    );

    QCOMPARE(
        updated_cam2.outcome_value,
        active_stream_state::settings_result::outcome::updated
    );
    QCOMPARE(updated_cam2.active_name, QStringLiteral("cam-2"));
    QCOMPARE(updated_cam2.previous_settings.labels_enabled, true);
    QVERIFY(updated_cam2.labels_changed);
    QVERIFY(updated_cam2.algorithm_changed);
    QVERIFY(!updated_cam2.processing_policy_changed);
    QCOMPARE(updated_cam2.settings.algorithm_id, QStringLiteral("contour_mask"));
    QCOMPARE(updated_cam2.settings.algorithm_preset, QStringLiteral("mask_heavy"));
    QVERIFY(updated_cam2.settings.algorithm_overlay_enabled);
    QCOMPARE(
        catalog_state.settings_for(QStringLiteral("cam-2")).algorithm_id,
        QStringLiteral("contour_mask")
    );
    QCOMPARE(board.active_cell()->current_stream_settings().stream_name, QStringLiteral("cam-2"));
    QCOMPARE(board.active_cell()->current_stream_settings().algorithm_id, QStringLiteral("contour_mask"));
    QCOMPARE(panel.current_active_stream_settings().labels_enabled, false);
    QCOMPARE(
        panel.current_active_stream_settings().algorithm_preset,
        QStringLiteral("mask_heavy")
    );

    const auto preset_update = active_streams.apply_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-2"),
            .labels_enabled = false,
            .algorithm_id = QStringLiteral("contour mask"),
            .algorithm_preset = QStringLiteral("balanced"),
            .algorithm_overlay_enabled = false,
        }
    );

    QCOMPARE(
        preset_update.outcome_value,
        active_stream_state::settings_result::outcome::updated
    );
    QVERIFY(!preset_update.labels_changed);
    QVERIFY(preset_update.algorithm_changed);
    QVERIFY(!preset_update.processing_policy_changed);
    QCOMPARE(preset_update.settings.algorithm_id, QStringLiteral("contour_mask"));
    QCOMPARE(preset_update.settings.algorithm_preset, QStringLiteral("balanced"));
    QVERIFY(!preset_update.settings.algorithm_overlay_enabled);

    const auto processing_update = active_streams.apply_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-2"),
            .labels_enabled = false,
            .algorithm_id = QStringLiteral("contour mask"),
            .algorithm_preset = QStringLiteral("balanced"),
            .algorithm_overlay_enabled = false,
            .manual_processing_policy_enabled = true,
            .manual_display_fps = 21,
            .manual_backend_fps = 8,
            .manual_processing_pixels = 640 * 360,
        }
    );

    QCOMPARE(
        processing_update.outcome_value,
        active_stream_state::settings_result::outcome::updated
    );
    QVERIFY(!processing_update.labels_changed);
    QVERIFY(!processing_update.algorithm_changed);
    QVERIFY(processing_update.processing_policy_changed);
    QVERIFY(processing_update.settings.manual_processing_policy_enabled);
    QCOMPARE(processing_update.settings.manual_display_fps, 21);
    QCOMPARE(processing_update.settings.manual_backend_fps, 8);
    QCOMPARE(processing_update.settings.manual_processing_pixels, 640 * 360);

    const auto switched_to_cam1 = active_streams.apply_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-1"),
            .labels_enabled = false,
            .algorithm_id = QStringLiteral("spot_grid"),
            .algorithm_preset = QStringLiteral("dense"),
            .algorithm_overlay_enabled = true,
        }
    );

    QCOMPARE(
        switched_to_cam1.outcome_value,
        active_stream_state::settings_result::outcome::switched_active_stream
    );
    QCOMPARE(switched_to_cam1.active_name, QStringLiteral("cam-1"));
    QCOMPARE(route_state.active_stream_name(), QStringLiteral("cam-1"));
    QVERIFY(board.active_cell() != nullptr);
    QCOMPARE(board.active_cell()->get_name(), QStringLiteral("cam-1"));
    QCOMPARE(
        switched_to_cam1.settings.algorithm_id,
        QStringLiteral("motion_baseline")
    );

    const auto cleared = active_streams.set_active_stream(QString());
    QCOMPARE(cleared.active_name, QString());
    QVERIFY(cleared.changed);
    QVERIFY(board.active_cell() == nullptr);
    QCOMPARE(panel.current_active_stream_settings().stream_name, QString());
}

void main_window_tests::active_stream_workflow_tracks_selection_logs_and_follow_up() {
    settings_panel panel;
    stream_board board;
    stream_widget_bridge widget_bridge(&board, &panel);
    active_edit_session edit_session;
    stream_catalog_state catalog_state;
    stream_route_state route_state;
    active_stream_state active_streams(
        catalog_state, route_state, widget_bridge, edit_session
    );
    active_stream_workflow workflow(active_streams);

    catalog_state.ensure_stream(QStringLiteral("cam-1"));
    catalog_state.ensure_stream(QStringLiteral("cam-2"));
    catalog_state.set_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-2"),
            .labels_enabled = true,
            .algorithm_id = QStringLiteral("contour mask"),
            .algorithm_preset = QStringLiteral("mask_heavy"),
            .algorithm_overlay_enabled = true,
        }
    );

    auto* cam1_tile = widget_bridge.show_stream_in_grid(
        QStringLiteral("cam-1"),
        catalog_state.settings_for(QStringLiteral("cam-1")), edit_session,
        stream_widget_bridge::grid_stream_binding {}
    );
    auto* cam2_tile = widget_bridge.show_stream_in_grid(
        QStringLiteral("cam-2"),
        catalog_state.settings_for(QStringLiteral("cam-2")), edit_session,
        stream_widget_bridge::grid_stream_binding {}
    );
    QVERIFY(cam1_tile != nullptr);
    QVERIFY(cam2_tile != nullptr);

    const auto selected_cam2 = workflow.set_active_stream(QStringLiteral("cam-2"));
    QCOMPARE(selected_cam2.entries.size(), 1);
    QCOMPARE(
        selected_cam2.entries.front().message,
        QStringLiteral("active stream selected")
    );
    QCOMPARE(
        selected_cam2.entries.front().stream_name,
        QStringLiteral("cam-2")
    );
    QCOMPARE(
        selected_cam2.entries.front().algorithm_id,
        QStringLiteral("contour_mask")
    );
    QVERIFY(selected_cam2.refresh_fps);
    QVERIFY(selected_cam2.update_monitor_inventory);
    QCOMPARE(selected_cam2.monitor_marker, QStringLiteral("active_stream_selected"));
    QCOMPARE(route_state.active_stream_name(), QStringLiteral("cam-2"));

    const auto updated_cam2 = workflow.apply_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-2"),
            .labels_enabled = false,
            .algorithm_id = QStringLiteral("contour mask"),
            .algorithm_preset = QStringLiteral("mask_heavy"),
            .algorithm_overlay_enabled = true,
        }
    );

    QCOMPARE(updated_cam2.entries.size(), 1);
    QCOMPARE(
        updated_cam2.entries.front().message,
        QStringLiteral("line labels disabled")
    );
    QCOMPARE(
        updated_cam2.entries.front().stream_name,
        QStringLiteral("cam-2")
    );
    QVERIFY(!updated_cam2.refresh_fps);
    QVERIFY(!updated_cam2.update_monitor_inventory);

    const auto preset_update = workflow.apply_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-2"),
            .labels_enabled = false,
            .algorithm_id = QStringLiteral("contour mask"),
            .algorithm_preset = QStringLiteral("balanced"),
            .algorithm_overlay_enabled = false,
        }
    );

    QCOMPARE(preset_update.entries.size(), 1);
    QCOMPARE(
        preset_update.entries.front().message,
        QStringLiteral("algorithm preference updated")
    );
    QVERIFY(
        preset_update.entries.front().detail.contains(
            QStringLiteral("backend runtime uses contour_mask")
        )
    );
    QVERIFY(
        preset_update.entries.front().detail.contains(
            QStringLiteral("preset=balanced")
        )
    );
    QVERIFY(
        preset_update.entries.front().detail.contains(
            QStringLiteral("overlay=false")
        )
    );
    QVERIFY(!preset_update.refresh_fps);

    const auto algorithm_update = workflow.apply_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-2"),
            .labels_enabled = false,
            .algorithm_id = QStringLiteral("spot_grid"),
            .algorithm_preset = QStringLiteral("dense"),
            .algorithm_overlay_enabled = false,
        }
    );

    QCOMPARE(algorithm_update.entries.size(), 1);
    QCOMPARE(
        algorithm_update.entries.front().message,
        QStringLiteral("algorithm preference updated")
    );
    QCOMPARE(
        algorithm_update.entries.front().severity,
        frontend_log_severity::info
    );
    QVERIFY(
        algorithm_update.entries.front().detail.contains(
            QStringLiteral("backend runtime uses spot_grid")
        )
    );
    QVERIFY(
        algorithm_update.entries.front().detail.contains(
            QStringLiteral("preset=dense")
        )
    );
    QCOMPARE(
        algorithm_update.entries.front().algorithm_id,
        QStringLiteral("spot_grid")
    );
    QVERIFY(!algorithm_update.refresh_fps);

    const auto processing_update = workflow.apply_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-2"),
            .labels_enabled = false,
            .algorithm_id = QStringLiteral("spot_grid"),
            .algorithm_preset = QStringLiteral("dense"),
            .algorithm_overlay_enabled = false,
            .manual_processing_policy_enabled = true,
            .manual_display_fps = 18,
            .manual_backend_fps = 9,
            .manual_processing_pixels = 320 * 180,
        }
    );

    QCOMPARE(processing_update.entries.size(), 1);
    QCOMPARE(
        processing_update.entries.front().message,
        QStringLiteral("processing tuning updated")
    );
    QVERIFY(
        processing_update.entries.front().detail.contains(
            QStringLiteral("display_fps=18")
        )
    );
    QVERIFY(processing_update.refresh_fps);
    QVERIFY(!processing_update.update_monitor_inventory);

    const auto switched_to_cam1 = workflow.apply_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-1"),
            .labels_enabled = false,
            .algorithm_id = QStringLiteral("spot_grid"),
            .algorithm_preset = QStringLiteral("dense"),
            .algorithm_overlay_enabled = true,
        }
    );

    QCOMPARE(switched_to_cam1.entries.size(), 1);
    QCOMPARE(
        switched_to_cam1.entries.front().message,
        QStringLiteral("active stream selected")
    );
    QCOMPARE(
        switched_to_cam1.entries.front().stream_name,
        QStringLiteral("cam-1")
    );
    QCOMPARE(
        switched_to_cam1.entries.front().algorithm_id,
        QStringLiteral("motion_baseline")
    );
    QVERIFY(switched_to_cam1.refresh_fps);
    QVERIFY(switched_to_cam1.update_monitor_inventory);
    QCOMPARE(switched_to_cam1.monitor_marker, QStringLiteral("active_stream_selected"));

    const auto cleared_by_empty = workflow.apply_stream_settings(
        stream_settings {}
    );
    QCOMPARE(cleared_by_empty.entries.size(), 1);
    QCOMPARE(
        cleared_by_empty.entries.front().message,
        QStringLiteral("active stream cleared")
    );
    QVERIFY(cleared_by_empty.refresh_fps);
    QVERIFY(cleared_by_empty.update_monitor_inventory);
    QCOMPARE(cleared_by_empty.monitor_marker, QStringLiteral("active_stream_cleared"));
    QVERIFY(board.active_cell() == nullptr);

    const auto ignored_empty = workflow.apply_stream_settings(
        stream_settings {}
    );
    QCOMPARE(ignored_empty.entries.size(), 0);
    QVERIFY(!ignored_empty.refresh_fps);
    QVERIFY(!ignored_empty.update_monitor_inventory);
}

void main_window_tests::active_edit_controller_tracks_editor_sync_and_resets() {
    settings_panel panel;
    stream_board board;
    stream_widget_bridge widget_bridge(&board, &panel);
    active_edit_session edit_session;
    active_edit_controller edit_controller(edit_session, widget_bridge);

    edit_session.set_draft_line_profile(
        line_profile {
            .name = QStringLiteral("north"),
            .color = QColor(Qt::cyan),
            .closed = true,
            .width_text = QStringLiteral("string_light"),
            .length_text = QStringLiteral("long"),
            .response_text = QStringLiteral("resonant"),
        }
    );
    edit_session.store_saved_line(
        QStringLiteral("cam-1"), QStringLiteral("north"),
        {
            QPointF(10.0, 10.0),
            QPointF(80.0, 20.0),
        },
        true
    );
    edit_session.reset_draft_line_profile();

    const stream_settings active_settings {
        .stream_name = QStringLiteral("cam-1"),
        .labels_enabled = true,
        .algorithm_id = QStringLiteral("motion_baseline"),
        .algorithm_preset = QStringLiteral("balanced"),
        .algorithm_overlay_enabled = false,
    };
    auto* grid_tile = widget_bridge.show_stream_in_grid(
        QStringLiteral("cam-1"), active_settings, edit_session,
        stream_widget_bridge::grid_stream_binding {
            .path = QString(),
            .type = QString(),
            .loop = true,
        }
    );
    QVERIFY(grid_tile != nullptr);
    widget_bridge.apply_active_stream(QStringLiteral("cam-1"), active_settings, edit_session);
    widget_bridge.sync_active_persistent(QStringLiteral("cam-1"), edit_session);

    const line_profile& line_value = edit_controller.apply_line_profile(
        line_profile {
            .name = QStringLiteral(" south "),
            .color = QColor(Qt::green),
            .closed = false,
            .width_text = QStringLiteral("string_heavy"),
            .length_text = QStringLiteral("short"),
            .response_text = QStringLiteral("dry"),
        }
    );

    QCOMPARE(line_value.name, QStringLiteral("south"));
    QCOMPARE(panel.current_active_line_profile().name, QStringLiteral("south"));
    QCOMPARE(
        panel.current_active_line_profile().width_text,
        QStringLiteral("string_heavy")
    );
    QVERIFY(board.active_cell() != nullptr);
    QCOMPARE(board.active_cell()->draft_name(), QStringLiteral("south"));
    QCOMPARE(board.active_cell()->draft_color(), QColor(Qt::green));

    board.active_cell()->set_draft_points_pct(
        {
            QPointF(1.0, 1.0),
            QPointF(2.0, 2.0),
            QPointF(3.0, 3.0),
        }
    );
    edit_controller.undo_last_draft_point();
    QCOMPARE(static_cast<int>(board.active_cell()->draft_points_pct().size()), 2);

    edit_controller.reset_after_line_saved(QStringLiteral("north"));
    QCOMPARE(panel.current_active_line_profile().name, QString());
    QCOMPARE(
        panel.current_active_line_profile().width_text,
        default_line_width_text()
    );

    auto* template_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_template_combo")
    );
    QVERIFY(template_combo != nullptr);
    QVERIFY(template_combo->findText(QStringLiteral("north")) >= 0);

    edit_controller.set_drawing_new_mode(false);
    const template_apply_settings& template_value
        = edit_controller.apply_template_settings(
            template_apply_settings {
                .template_name = QStringLiteral("north"),
                .color = QColor(Qt::yellow),
                .width_text = QStringLiteral("thin"),
                .length_text = QStringLiteral("short"),
                .response_text = QStringLiteral("dry"),
            }
        );

    QCOMPARE(template_value.template_name, QStringLiteral("north"));
    QCOMPARE(
        panel.current_active_template_settings().template_name,
        QStringLiteral("north")
    );
    QCOMPARE(board.active_cell()->draft_name(), QStringLiteral("north"));
    QCOMPARE(board.active_cell()->draft_color(), QColor(Qt::yellow));
    QCOMPARE(static_cast<int>(board.active_cell()->draft_points_pct().size()), 2);

    edit_controller.reset_after_template_applied();
    QCOMPARE(panel.current_active_template_settings().template_name, QString());
    QCOMPARE(static_cast<int>(board.active_cell()->draft_points_pct().size()), 0);
}

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
    QVERIFY(!fallback_settings.manual_processing_policy_enabled);
    QCOMPARE(fallback_settings.manual_display_fps, default_manual_display_fps());
    QCOMPARE(fallback_settings.manual_backend_fps, default_manual_backend_fps());
    QCOMPARE(
        fallback_settings.manual_processing_pixels,
        default_manual_processing_pixels()
    );

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
            .manual_processing_policy_enabled = true,
            .manual_display_fps = 37,
            .manual_backend_fps = 11,
            .manual_processing_pixels = 640 * 360,
        }
    );

    const stream_settings saved_settings
        = catalog_state.settings_for(QStringLiteral("cam-1"));
    QCOMPARE(saved_settings.stream_name, QStringLiteral("cam-1"));
    QVERIFY(!saved_settings.labels_enabled);
    QCOMPARE(saved_settings.algorithm_id, QStringLiteral("contour_mask"));
    QCOMPARE(saved_settings.algorithm_preset, QStringLiteral("mask_heavy"));
    QVERIFY(saved_settings.algorithm_overlay_enabled);
    QVERIFY(saved_settings.manual_processing_policy_enabled);
    QCOMPARE(saved_settings.manual_display_fps, 37);
    QCOMPARE(saved_settings.manual_backend_fps, 11);
    QCOMPARE(saved_settings.manual_processing_pixels, 640 * 360);

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

void main_window_tests::stream_catalog_workflow_tracks_seed_add_and_local_sources() {
    settings_panel panel;
    stream_board board;
    stream_widget_bridge widget_bridge(&board, &panel);
    stream_catalog_state catalog_state;
    yodau::backend::stream_manager stream_mgr;
    stream_catalog_workflow workflow(
        &stream_mgr, &panel, catalog_state, widget_bridge
    );

    stream_mgr.add_stream("/tmp/cam-1.mp4", "cam-1", "file", true);
    workflow.seed_from_backend();

    auto* streams_list = panel.findChild<QTreeWidget*>();
    QVERIFY(streams_list != nullptr);
    QCOMPARE(streams_list->topLevelItemCount(), 1);
    QCOMPARE(streams_list->topLevelItem(0)->text(1), QStringLiteral("cam-1"));
    QCOMPARE(
        catalog_state.settings_for(QStringLiteral("cam-1")).stream_name,
        QStringLiteral("cam-1")
    );

    workflow.seed_from_backend();
    QCOMPARE(streams_list->topLevelItemCount(), 1);

    const auto add_result = workflow.add_stream(
        QStringLiteral("/tmp/cam-2.mp4"), QStringLiteral("cam-2"),
        QStringLiteral("file"), true
    );
    QCOMPARE(add_result.entries.size(), 1);
    QCOMPARE(
        add_result.entries.front().message, QStringLiteral("stream added")
    );
    QCOMPARE(
        add_result.entries.front().stream_name, QStringLiteral("cam-2")
    );
    QCOMPARE(
        add_result.entries.front().detail,
        QStringLiteral("file:/tmp/cam-2.mp4")
    );
    QVERIFY(add_result.refresh_fps);
    QVERIFY(add_result.update_monitor_inventory);
    QCOMPARE(add_result.monitor_marker, QStringLiteral("stream_added"));
    QCOMPARE(streams_list->topLevelItemCount(), 2);
    QCOMPARE(
        catalog_state.settings_for(QStringLiteral("cam-2")).stream_name,
        QStringLiteral("cam-2")
    );

    const auto invalid_url_result = workflow.add_stream(
        QStringLiteral("not-a-url"), QStringLiteral("cam-url"),
        QStringLiteral("url"), true
    );
    QCOMPARE(invalid_url_result.entries.size(), 1);
    QCOMPARE(
        invalid_url_result.entries.front().message, QStringLiteral("invalid url")
    );
    QVERIFY(!invalid_url_result.refresh_fps);
    QCOMPARE(streams_list->topLevelItemCount(), 2);

    const auto local_result = workflow.detect_local_sources();
    QCOMPARE(local_result.entries.size(), 1);
    QCOMPARE(
        local_result.entries.front().message,
        QStringLiteral("local source inventory refreshed")
    );
    QVERIFY(
        local_result.entries.front().detail.contains(QStringLiteral("backend="))
    );
    QVERIFY(
        local_result.entries.front().detail.contains(QStringLiteral("qt="))
    );
    QVERIFY(!local_result.refresh_fps);
    QVERIFY(local_result.update_monitor_inventory);
    QCOMPARE(local_result.monitor_marker, QStringLiteral("local_sources_refreshed"));

    auto* local_sources_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_add_local_sources_combo")
    );
    QVERIFY(local_sources_combo != nullptr);
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

void main_window_tests::stream_widget_bridge_syncs_active_editor_controls() {
    settings_panel panel;
    stream_board board;
    stream_widget_bridge widget_bridge(&board, &panel);
    active_edit_session edit_session;

    edit_session.set_draft_line_profile(
        line_profile {
            .name = QStringLiteral(" north "),
            .color = QColor(Qt::green),
            .closed = true,
            .width_text = QStringLiteral("string_heavy"),
            .length_text = QStringLiteral("long"),
            .response_text = QStringLiteral("resonant"),
        }
    );
    edit_session.set_active_template_settings(
        template_apply_settings {
            .template_name = QStringLiteral("north"),
            .color = QColor(Qt::yellow),
            .width_text = QStringLiteral("thin"),
            .length_text = QStringLiteral("short"),
            .response_text = QStringLiteral("dry"),
        }
    );

    widget_bridge.initialize_editor_state(edit_session);

    QCOMPARE(panel.current_active_line_profile().name, QStringLiteral("north"));
    QCOMPARE(
        panel.current_active_line_profile().width_text,
        QStringLiteral("string_heavy")
    );
    QCOMPARE(
        panel.current_active_template_settings().response_text,
        QStringLiteral("dry")
    );

    widget_bridge.add_template_candidate(QStringLiteral("north"));
    widget_bridge.sync_active_template_editor(edit_session);
    auto* template_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_template_combo")
    );
    QVERIFY(template_combo != nullptr);
    QVERIFY(template_combo->findText(QStringLiteral("north")) >= 0);
    QCOMPARE(
        panel.current_active_template_settings().template_name,
        QStringLiteral("north")
    );

    edit_session.reset_draft_line_profile();
    widget_bridge.sync_active_line_editor(edit_session, true);
    QCOMPARE(panel.current_active_line_profile().name, QString());
    QCOMPARE(
        panel.current_active_line_profile().width_text,
        default_line_width_text()
    );

    edit_session.reset_active_template_settings();
    widget_bridge.sync_active_template_editor(edit_session, true);
    QCOMPARE(
        panel.current_active_template_settings().template_name, QString()
    );
    QCOMPARE(
        panel.current_active_template_settings().width_text,
        default_line_width_text()
    );
}

void main_window_tests::
    stream_widget_bridge_registers_and_routes_grid_visibility() {
    settings_panel panel;
    stream_board board;
    stream_widget_bridge widget_bridge(&board, &panel);
    active_edit_session edit_session;

    widget_bridge.register_stream_entry(
        QStringLiteral("cam-1"), QStringLiteral("file:/tmp/demo.mp4")
    );
    widget_bridge.register_stream_entry(
        QStringLiteral("cam-2"), QStringLiteral("url:rtsp://cam-2")
    );

    auto* streams_list = panel.findChild<QTreeWidget*>();
    QVERIFY(streams_list != nullptr);
    QCOMPARE(streams_list->topLevelItemCount(), 2);
    QCOMPARE(streams_list->topLevelItem(0)->text(1), QStringLiteral("cam-1"));
    QCOMPARE(
        streams_list->topLevelItem(0)->text(2),
        QStringLiteral("file:/tmp/demo.mp4")
    );

    const stream_settings grid_settings {
        .stream_name = QStringLiteral("cam-1"),
        .labels_enabled = false,
        .algorithm_id = QStringLiteral("spot grid"),
        .algorithm_preset = QStringLiteral("fast"),
        .algorithm_overlay_enabled = false,
    };

    auto* grid_tile = widget_bridge.show_stream_in_grid(
        QStringLiteral("cam-1"), grid_settings, edit_session,
        stream_widget_bridge::grid_stream_binding {
            .path = QString(),
            .type = QString(),
            .loop = false,
        }
    );
    QVERIFY(grid_tile != nullptr);
    QVERIFY(board.grid_mode()->has_stream(QStringLiteral("cam-1")));
    QCOMPARE(
        grid_tile->current_stream_settings().algorithm_id,
        QStringLiteral("spot_grid")
    );

    auto* active_tile = widget_bridge.show_stream_in_grid(
        QStringLiteral("cam-2"),
        stream_settings {
            .stream_name = QStringLiteral("cam-2"),
            .labels_enabled = true,
            .algorithm_id = QStringLiteral("motion_baseline"),
            .algorithm_preset = QStringLiteral("balanced"),
            .algorithm_overlay_enabled = true,
        },
        edit_session,
        stream_widget_bridge::grid_stream_binding {
            .path = QString(),
            .type = QString(),
            .loop = true,
        }
    );
    QVERIFY(active_tile != nullptr);
    QVERIFY(board.grid_mode()->has_stream(QStringLiteral("cam-2")));

    widget_bridge.apply_active_stream(
        QStringLiteral("cam-2"),
        stream_settings {
            .stream_name = QStringLiteral("cam-2"),
            .labels_enabled = true,
            .algorithm_id = QStringLiteral("motion_baseline"),
            .algorithm_preset = QStringLiteral("balanced"),
            .algorithm_overlay_enabled = true,
        },
        edit_session
    );
    QVERIFY(board.active_cell() != nullptr);
    QCOMPARE(board.active_cell()->get_name(), QStringLiteral("cam-2"));
    QCOMPARE(panel.current_active_stream_settings().stream_name, QStringLiteral("cam-2"));

    widget_bridge.hide_stream_from_grid(QStringLiteral("cam-1"), false);
    QVERIFY(!board.grid_mode()->has_stream(QStringLiteral("cam-1")));
    QVERIFY(board.active_cell() != nullptr);
    QCOMPARE(board.active_cell()->get_name(), QStringLiteral("cam-2"));

    widget_bridge.hide_stream_from_grid(QStringLiteral("cam-2"), true);
    QVERIFY(board.active_cell() == nullptr);
    QCOMPARE(panel.current_active_stream_settings().stream_name, QString());
}

void main_window_tests::stream_widget_bridge_syncs_visible_log_mode() {
    settings_panel panel;
    panel.set_log_mode(frontend_log_mode::debug);

    stream_board board;
    stream_widget_bridge widget_bridge(&board, &panel);
    active_edit_session edit_session;

    auto* cam1_tile = widget_bridge.show_stream_in_grid(
        QStringLiteral("cam-1"),
        stream_settings {
            .stream_name = QStringLiteral("cam-1"),
            .labels_enabled = true,
            .algorithm_id = QStringLiteral("motion_baseline"),
            .algorithm_preset = QStringLiteral("balanced"),
            .algorithm_overlay_enabled = false,
        },
        edit_session, stream_widget_bridge::grid_stream_binding {}
    );
    auto* cam2_tile = widget_bridge.show_stream_in_grid(
        QStringLiteral("cam-2"),
        stream_settings {
            .stream_name = QStringLiteral("cam-2"),
            .labels_enabled = true,
            .algorithm_id = QStringLiteral("contour_mask"),
            .algorithm_preset = QStringLiteral("mask_heavy"),
            .algorithm_overlay_enabled = true,
        },
        edit_session, stream_widget_bridge::grid_stream_binding {}
    );

    QVERIFY(cam1_tile != nullptr);
    QVERIFY(cam2_tile != nullptr);
    QCOMPARE(cam1_tile->current_log_mode(), frontend_log_mode::debug);
    QCOMPARE(cam2_tile->current_log_mode(), frontend_log_mode::debug);

    board.set_active_stream(QStringLiteral("cam-2"));
    QVERIFY(board.active_cell() != nullptr);
    QCOMPARE(board.active_cell()->current_log_mode(), frontend_log_mode::debug);

    panel.set_log_mode(frontend_log_mode::release);
    widget_bridge.sync_visible_log_mode(panel.log_mode());

    QCOMPARE(cam1_tile->current_log_mode(), frontend_log_mode::release);
    QCOMPARE(board.active_cell()->current_log_mode(), frontend_log_mode::release);
}

void main_window_tests::
    stream_inventory_panel_tracks_entries_and_visibility_signal() {
    stream_inventory_panel panel;
    QSignalSpy show_spy(
        &panel, &stream_inventory_panel::show_stream_changed
    );

    panel.add_stream_entry(
        QStringLiteral("cam-1"), QStringLiteral("file:/tmp/demo.mp4")
    );
    panel.add_stream_entry(
        QStringLiteral("cam-2"), QStringLiteral("url:rtsp://cam-2"), true
    );

    auto* streams_list = panel.findChild<QTreeWidget*>();
    QVERIFY(streams_list != nullptr);
    QCOMPARE(streams_list->topLevelItemCount(), 2);
    QCOMPARE(streams_list->topLevelItem(0)->text(1), QStringLiteral("cam-1"));
    QCOMPARE(streams_list->topLevelItem(1)->checkState(0), Qt::Checked);

    streams_list->topLevelItem(0)->setCheckState(0, Qt::Checked);
    QCOMPARE(show_spy.size(), 1);
    QCOMPARE(show_spy.at(0).at(0).toString(), QStringLiteral("cam-1"));
    QCOMPARE(show_spy.at(0).at(1).toBool(), true);

    panel.remove_stream_entry(QStringLiteral("cam-2"));
    QCOMPARE(streams_list->topLevelItemCount(), 1);

    panel.clear_stream_entries();
    QCOMPARE(streams_list->topLevelItemCount(), 0);
}

void main_window_tests::
    stream_source_panel_tracks_modes_validation_and_requests() {
    qRegisterMetaType<frontend_log_entry>("frontend_log_entry");

    stream_source_panel panel;
    panel.set_existing_names({ QStringLiteral("dup") });
    panel.set_local_sources({ QStringLiteral("video0") });

    auto* name_edit = panel.findChild<QLineEdit*>(
        QStringLiteral("settings_add_name_edit")
    );
    auto* local_radio = panel.findChild<QRadioButton*>(
        QStringLiteral("settings_add_local_radio")
    );
    auto* url_radio = panel.findChild<QRadioButton*>(
        QStringLiteral("settings_add_url_radio")
    );
    auto* local_sources_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_add_local_sources_combo")
    );
    auto* refresh_local_button = panel.findChild<QPushButton*>(
        QStringLiteral("settings_add_refresh_local_button")
    );
    auto* url_edit = panel.findChild<QLineEdit*>(
        QStringLiteral("settings_add_url_edit")
    );
    auto* add_button = panel.findChild<QPushButton*>(
        QStringLiteral("settings_add_button")
    );

    QVERIFY(name_edit != nullptr);
    QVERIFY(local_radio != nullptr);
    QVERIFY(url_radio != nullptr);
    QVERIFY(local_sources_combo != nullptr);
    QVERIFY(refresh_local_button != nullptr);
    QVERIFY(url_edit != nullptr);
    QVERIFY(add_button != nullptr);

    QSignalSpy add_local_spy(&panel, &stream_source_panel::add_local_stream);
    QSignalSpy add_url_spy(&panel, &stream_source_panel::add_url_stream);
    QSignalSpy refresh_spy(
        &panel, &stream_source_panel::detect_local_sources_requested
    );
    QSignalSpy log_spy(&panel, &stream_source_panel::log_requested);

    local_radio->click();
    name_edit->setText(QStringLiteral("dup"));
    QVERIFY(!add_button->isEnabled());

    name_edit->setText(QStringLiteral(" cam-1 "));
    QCOMPARE(local_sources_combo->currentText(), QStringLiteral("video0"));
    QVERIFY(add_button->isEnabled());

    add_button->click();
    QCOMPARE(add_local_spy.size(), 1);
    QCOMPARE(add_local_spy.at(0).at(0).toString(), QStringLiteral("video0"));
    QCOMPARE(add_local_spy.at(0).at(1).toString(), QStringLiteral("cam-1"));
    QVERIFY(log_spy.size() >= 1);
    QCOMPARE(
        qvariant_cast<frontend_log_entry>(log_spy.at(0).at(0)).message,
        QStringLiteral("requested local stream add")
    );

    refresh_local_button->click();
    QCOMPARE(refresh_spy.size(), 1);
    QVERIFY(log_spy.size() >= 2);
    QCOMPARE(
        qvariant_cast<frontend_log_entry>(log_spy.at(1).at(0)).message,
        QStringLiteral("local source detection requested")
    );

    url_radio->click();
    QVERIFY(!add_button->isEnabled());

    url_edit->setText(QStringLiteral("rtsp://example.test/live"));
    QVERIFY(add_button->isEnabled());
    add_button->click();
    QCOMPARE(add_url_spy.size(), 1);
    QCOMPARE(
        add_url_spy.at(0).at(0).toString(),
        QStringLiteral("rtsp://example.test/live")
    );
    QCOMPARE(add_url_spy.at(0).at(1).toString(), QStringLiteral("cam-1"));
    QVERIFY(log_spy.size() >= 3);
    QCOMPARE(
        qvariant_cast<frontend_log_entry>(log_spy.at(2).at(0)).message,
        QStringLiteral("requested url stream add")
    );

    panel.clear_inputs();
    QCOMPARE(name_edit->text(), QString());
    QCOMPARE(url_edit->text(), QString());
    QVERIFY(!add_button->isEnabled());
}

void main_window_tests::
    active_stream_panel_tracks_selection_modes_and_settings() {
    active_stream_panel panel;
    panel.set_active_candidates({ QStringLiteral("cam-1"), QStringLiteral("cam-2") });

    QSignalSpy stream_settings_spy(
        &panel, &active_stream_panel::stream_settings_changed
    );
    QSignalSpy edit_mode_spy(&panel, &active_stream_panel::edit_mode_changed);

    auto* active_stream_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_stream_combo")
    );
    auto* active_labels_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_active_labels_checkbox")
    );
    auto* operator_profile_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_operator_profile_combo")
    );
    auto* operator_profile_summary = panel.findChild<QLabel*>(
        QStringLiteral("settings_active_operator_profile_summary_label")
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
    auto* manual_processing_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_active_manual_processing_checkbox")
    );
    auto* manual_display_fps_spin = panel.findChild<QSpinBox*>(
        QStringLiteral("settings_active_display_fps_spin")
    );
    auto* manual_backend_fps_spin = panel.findChild<QSpinBox*>(
        QStringLiteral("settings_active_backend_fps_spin")
    );
    auto* manual_processing_pixels_spin = panel.findChild<QSpinBox*>(
        QStringLiteral("settings_active_processing_pixels_spin")
    );
    auto* template_radio = panel.findChild<QRadioButton*>(
        QStringLiteral("settings_active_mode_template_radio")
    );

    QVERIFY(active_stream_combo != nullptr);
    QVERIFY(active_labels_checkbox != nullptr);
    QVERIFY(operator_profile_combo != nullptr);
    QVERIFY(operator_profile_summary != nullptr);
    QVERIFY(algorithm_combo != nullptr);
    QVERIFY(preset_combo != nullptr);
    QVERIFY(overlay_checkbox != nullptr);
    QVERIFY(manual_processing_checkbox != nullptr);
    QVERIFY(manual_display_fps_spin != nullptr);
    QVERIFY(manual_backend_fps_spin != nullptr);
    QVERIFY(manual_processing_pixels_spin != nullptr);
    QVERIFY(template_radio != nullptr);

    QVERIFY(!panel.has_active_stream());
    QVERIFY(panel.drawing_new_mode());

    active_stream_combo->setCurrentText(QStringLiteral("cam-1"));
    QCOMPARE(stream_settings_spy.size(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const stream_settings settings_value
            = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(settings_value.labels_enabled, true);
        QCOMPARE(
            settings_value.algorithm_id, QStringLiteral("motion_baseline")
        );
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("balanced"));
        QVERIFY(!settings_value.manual_processing_policy_enabled);
        QCOMPARE(
            settings_value.manual_display_fps, default_manual_display_fps()
        );
        QCOMPARE(
            settings_value.manual_backend_fps, default_manual_backend_fps()
        );
        QCOMPARE(
            settings_value.manual_processing_pixels,
            default_manual_processing_pixels()
        );
    }
    QVERIFY(panel.has_active_stream());
    QCOMPARE(
        operator_profile_combo->currentData().toString(),
        QStringLiteral("balanced")
    );
    QVERIFY(
        operator_profile_summary->text().contains(QStringLiteral("balanced"))
    );

    const int contour_mask_index
        = algorithm_combo->findData(QStringLiteral("contour_mask"));
    QVERIFY(contour_mask_index >= 0);
    algorithm_combo->setCurrentIndex(contour_mask_index);
    QCOMPARE(stream_settings_spy.size(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const stream_settings settings_value
            = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(
            settings_value.algorithm_id, QStringLiteral("contour_mask")
        );
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("balanced"));
    }
    QCOMPARE(
        preset_combo->currentData().toString(), QStringLiteral("balanced")
    );
    QCOMPARE(
        operator_profile_combo->currentData().toString(),
        QStringLiteral("balanced")
    );

    const int debug_heavy_index
        = operator_profile_combo->findData(QStringLiteral("debug_heavy"));
    QVERIFY(debug_heavy_index >= 0);
    operator_profile_combo->setCurrentIndex(debug_heavy_index);
    QCOMPARE(stream_settings_spy.size(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const stream_settings settings_value
            = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("mask_heavy"));
        QVERIFY(settings_value.algorithm_overlay_enabled);
    }
    QCOMPARE(
        preset_combo->currentData().toString(), QStringLiteral("mask_heavy")
    );
    QVERIFY(
        operator_profile_summary->text().contains(QStringLiteral("debug-heavy"))
    );

    overlay_checkbox->setChecked(false);
    QCOMPARE(stream_settings_spy.size(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const stream_settings settings_value
            = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("mask_heavy"));
        QVERIFY(!settings_value.algorithm_overlay_enabled);
    }
    QCOMPARE(
        operator_profile_combo->currentData().toString(),
        QStringLiteral("custom")
    );

    active_labels_checkbox->setChecked(false);
    QCOMPARE(stream_settings_spy.size(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const stream_settings settings_value
            = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QVERIFY(!settings_value.labels_enabled);
        QCOMPARE(
            settings_value.algorithm_id, QStringLiteral("contour_mask")
        );
    }

    manual_processing_checkbox->setChecked(true);
    QCOMPARE(stream_settings_spy.size(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const stream_settings settings_value
            = qvariant_cast<stream_settings>(args.at(0));
        QVERIFY(settings_value.manual_processing_policy_enabled);
        QCOMPARE(
            settings_value.manual_display_fps, default_manual_display_fps()
        );
    }

    manual_display_fps_spin->setValue(18);
    manual_backend_fps_spin->setValue(9);
    manual_processing_pixels_spin->setValue(320 * 180);
    QCOMPARE(stream_settings_spy.size(), 3);
    {
        const auto args = stream_settings_spy.takeLast();
        const stream_settings settings_value
            = qvariant_cast<stream_settings>(args.at(0));
        QVERIFY(settings_value.manual_processing_policy_enabled);
        QCOMPARE(settings_value.manual_display_fps, 18);
        QCOMPARE(settings_value.manual_backend_fps, 9);
        QCOMPARE(settings_value.manual_processing_pixels, 320 * 180);
    }
    stream_settings_spy.clear();

    template_radio->click();
    QCOMPARE(edit_mode_spy.size(), 1);
    QCOMPARE(edit_mode_spy.at(0).at(0).toBool(), false);
    QVERIFY(!panel.drawing_new_mode());

    panel.set_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-2"),
            .labels_enabled = true,
            .algorithm_id = QStringLiteral("spot grid"),
            .algorithm_preset = QStringLiteral("dense"),
            .algorithm_overlay_enabled = false,
        }
    );
    const stream_settings current_settings = panel.current_stream_settings();
    QCOMPARE(current_settings.stream_name, QStringLiteral("cam-2"));
    QCOMPARE(current_settings.labels_enabled, true);
    QCOMPARE(current_settings.algorithm_id, QStringLiteral("spot_grid"));
    QCOMPARE(current_settings.algorithm_preset, QStringLiteral("dense"));
    QCOMPARE(current_settings.algorithm_overlay_enabled, false);
}

void main_window_tests::
    log_toolbar_panel_filters_formats_and_emits_actions() {
    log_toolbar_panel panel;
    frontend_log_buffer buffer;
    panel.set_log_buffer(&buffer);

    QSignalSpy copy_logs_spy(&panel, &log_toolbar_panel::copy_logs_requested);
    QSignalSpy copy_summary_spy(
        &panel, &log_toolbar_panel::copy_summary_requested
    );
    QSignalSpy save_logs_spy(&panel, &log_toolbar_panel::save_logs_requested);

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
    QVERIFY(severity_filter_combo != nullptr);
    QVERIFY(stream_filter_combo != nullptr);
    QVERIFY(subsystem_filter_combo != nullptr);
    QVERIFY(copy_logs_button != nullptr);
    QVERIFY(copy_summary_button != nullptr);
    QVERIFY(save_logs_button != nullptr);
    QCOMPARE(panel.log_mode(), frontend_log_mode::release);

    const QDateTime timestamp(
        QDate(2026, 3, 29), QTime(14, 0, 0, 0), QTimeZone::UTC
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, frontend_log_area::add, frontend_log_severity::debug,
            QStringLiteral("tests"), QStringLiteral("debug add"),
            QStringLiteral("cam-1"), QStringLiteral("hidden in release")
        )
    );
    buffer.append(
        main_window_tests_support::make_log_entry(
            timestamp, frontend_log_area::add, frontend_log_severity::info,
            QStringLiteral("tests"), QStringLiteral("info add"),
            QStringLiteral("cam-1")
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
            QStringLiteral("cam-2")
        )
    );

    const QString add_release_text
        = panel.formatted_entries(frontend_log_area::add).join('\n');
    QVERIFY(add_release_text.contains(QStringLiteral("info add")));
    QVERIFY(!add_release_text.contains(QStringLiteral("debug add")));

    log_mode_combo->setCurrentIndex(1);
    QCOMPARE(panel.log_mode(), frontend_log_mode::debug);
    const QString add_debug_text
        = panel.formatted_entries(frontend_log_area::add).join('\n');
    QVERIFY(add_debug_text.contains(QStringLiteral("debug add")));
    QVERIFY(add_debug_text.contains(QStringLiteral("area=add")));

    const int backend_event_index
        = subsystem_filter_combo->findData(QStringLiteral("backend_event"));
    const int cam2_index = stream_filter_combo->findData(QStringLiteral("cam-2"));
    const int error_index = severity_filter_combo->findData(
        static_cast<int>(frontend_log_severity::error)
    );
    QVERIFY(backend_event_index >= 0);
    QVERIFY(cam2_index >= 0);
    QVERIFY(error_index >= 0);

    subsystem_filter_combo->setCurrentIndex(backend_event_index);
    stream_filter_combo->setCurrentIndex(cam2_index);
    severity_filter_combo->setCurrentIndex(error_index);

    const QString active_error_report
        = panel.compose_log_report(frontend_log_area::active);
    QVERIFY(active_error_report.contains(QStringLiteral("active error")));
    QVERIFY(!active_error_report.contains(QStringLiteral("active info")));

    const QString active_error_summary
        = panel.compose_log_summary(frontend_log_area::active);
    QVERIFY(active_error_summary.contains(QStringLiteral("entries=1")));
    QVERIFY(active_error_summary.contains(QStringLiteral("error=1")));

    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());
    const QString report_path = temp_dir.filePath(QStringLiteral("log-report.txt"));
    QVERIFY(panel.write_log_report(frontend_log_area::active, report_path));

    QFile saved_report(report_path);
    QVERIFY(saved_report.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString saved_text = QString::fromUtf8(saved_report.readAll());
    QVERIFY(saved_text.contains(QStringLiteral("active error")));

    copy_logs_button->click();
    copy_summary_button->click();
    save_logs_button->click();
    QCOMPARE(copy_logs_spy.size(), 1);
    QCOMPARE(copy_summary_spy.size(), 1);
    QCOMPARE(save_logs_spy.size(), 1);
}

void main_window_tests::active_editor_panel_tracks_active_tools_and_log() {
    active_editor_panel panel;
    frontend_log_buffer buffer;
    log_toolbar_panel toolbar;

    toolbar.set_log_buffer(&buffer);
    panel.set_log_toolbar(&toolbar);
    panel.set_active_candidates({ QStringLiteral("cam-1") });
    panel.set_template_candidates({ QStringLiteral("north") });
    panel.set_active_current(QStringLiteral("cam-1"));

    auto* line_panel = panel.findChild<QGroupBox*>(
        QStringLiteral("settings_active_line_profile_panel")
    );
    auto* template_panel = panel.findChild<QGroupBox*>(
        QStringLiteral("settings_active_template_apply_panel")
    );
    auto* template_radio = panel.findChild<QRadioButton*>(
        QStringLiteral("settings_active_mode_template_radio")
    );
    auto* active_log_view = panel.findChild<QPlainTextEdit*>(
        QStringLiteral("settings_active_log_view")
    );

    QVERIFY(line_panel != nullptr);
    QVERIFY(template_panel != nullptr);
    QVERIFY(template_radio != nullptr);
    QVERIFY(active_log_view != nullptr);

    QVERIFY(!line_panel->isHidden());
    QVERIFY(line_panel->isEnabled());
    QVERIFY(template_panel->isHidden());

    template_radio->click();
    QVERIFY(line_panel->isHidden());
    QVERIFY(!template_panel->isHidden());
    QVERIFY(template_panel->isEnabled());

    const frontend_log_entry entry = main_window_tests_support::make_log_entry(
        QDateTime(
            QDate(2026, 3, 29), QTime(14, 45, 0, 0), QTimeZone::UTC
        ),
        frontend_log_area::active, frontend_log_severity::warning,
        QStringLiteral("backend_event"), QStringLiteral("template preview"),
        QStringLiteral("cam-1"), QStringLiteral("line=north")
    );

    buffer.append(entry);
    const QString log_text = active_log_view->toPlainText();
    QVERIFY(log_text.contains(QStringLiteral("template preview")));
    QVERIFY(!log_text.contains(QStringLiteral("area=active")));
}

void main_window_tests::log_area_view_tracks_toolbar_refresh_and_fallback_append() {
    frontend_log_buffer buffer;
    log_toolbar_panel toolbar;
    toolbar.set_log_buffer(&buffer);

    log_area_view active_view(frontend_log_area::active);
    active_view.set_log_toolbar(&toolbar);

    auto* stream_filter_combo = toolbar.findChild<QComboBox*>(
        QStringLiteral("settings_log_stream_filter_combo")
    );
    QVERIFY(stream_filter_combo != nullptr);

    const QDateTime timestamp(
        QDate(2026, 3, 29), QTime(14, 5, 6, 7), QTimeZone::UTC
    );
    const frontend_log_entry active_debug
        = main_window_tests_support::make_log_entry(
            timestamp, frontend_log_area::active, frontend_log_severity::debug,
            QStringLiteral("backend_event"), QStringLiteral("debug motion"),
            QStringLiteral("cam-1"), QStringLiteral("cells=9"),
            QStringLiteral("spot_grid")
        );
    const frontend_log_entry active_warning
        = main_window_tests_support::make_log_entry(
            timestamp, frontend_log_area::active,
            frontend_log_severity::warning, QStringLiteral("backend_event"),
            QStringLiteral("tripwire warning"), QStringLiteral("cam-2")
        );
    const frontend_log_entry streams_error
        = main_window_tests_support::make_log_entry(
            timestamp, frontend_log_area::streams,
            frontend_log_severity::error, QStringLiteral("grid_visibility"),
            QStringLiteral("streams error"), QStringLiteral("cam-2")
        );

    buffer.append(active_debug);
    buffer.append(active_warning);
    buffer.append(streams_error);

    const QString active_release_text = active_view.toPlainText();
    QVERIFY(!active_release_text.contains(QStringLiteral("debug motion")));
    QVERIFY(active_release_text.contains(QStringLiteral("tripwire warning")));
    QVERIFY(!active_release_text.contains(QStringLiteral("streams error")));

    toolbar.set_log_mode(frontend_log_mode::debug);
    const QString active_debug_text = active_view.toPlainText();
    QVERIFY(active_debug_text.contains(QStringLiteral("debug motion")));
    QVERIFY(active_debug_text.contains(QStringLiteral("area=active")));
    QVERIFY(!active_debug_text.contains(QStringLiteral("streams error")));

    const int cam2_index = stream_filter_combo->findData(QStringLiteral("cam-2"));
    QVERIFY(cam2_index >= 0);
    stream_filter_combo->setCurrentIndex(cam2_index);
    const QString cam2_text = active_view.toPlainText();
    QVERIFY(!cam2_text.contains(QStringLiteral("debug motion")));
    QVERIFY(cam2_text.contains(QStringLiteral("tripwire warning")));

    log_area_view add_view(frontend_log_area::add);
    const frontend_log_entry add_warning = main_window_tests_support::make_log_entry(
        timestamp, frontend_log_area::add, frontend_log_severity::warning,
        QStringLiteral("stream_source_panel"), QStringLiteral("invalid url"),
        QStringLiteral("cam-url"), QStringLiteral("missing scheme")
    );

    QVERIFY(add_view.append_entry(add_warning));
    QCOMPARE(
        add_view.toPlainText(),
        format_frontend_log_entry(frontend_log_mode::release, add_warning)
    );
    QVERIFY(!add_view.append_entry(active_warning));
    QCOMPARE(
        add_view.toPlainText(),
        format_frontend_log_entry(frontend_log_mode::release, add_warning)
    );
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
        normalized_operator_profile_id(QStringLiteral("debug-heavy")),
        QStringLiteral("debug_heavy")
    );
    QCOMPARE(
        [] {
            stream_settings settings_value;
            settings_value.algorithm_id = QStringLiteral("spot_grid");
            return apply_operator_profile(
                       settings_value, QStringLiteral("simple")
            )
                .algorithm_preset;
        }(),
        QStringLiteral("coarse")
    );
    QCOMPARE(
        [] {
            stream_settings settings_value;
            settings_value.algorithm_id = QStringLiteral("contour_mask");
            settings_value.algorithm_preset = QStringLiteral("mask_heavy");
            settings_value.algorithm_overlay_enabled = true;
            return inferred_operator_profile_id(settings_value);
        }(),
        QStringLiteral("debug_heavy")
    );
    QVERIFY(
        [] {
            stream_settings settings_value;
            settings_value.algorithm_id = QStringLiteral("contour_mask");
            settings_value.algorithm_preset = QStringLiteral("mask_heavy");
            settings_value.algorithm_overlay_enabled = true;
            return operator_profile_summary_text(settings_value);
        }()
            .contains(QStringLiteral("debug-heavy"))
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
            .manual_processing_policy_enabled = true,
            .manual_display_fps = 19,
            .manual_backend_fps = 8,
            .manual_processing_pixels = 640 * 360,
        }
    );

    const stream_settings active_stream = panel.current_active_stream_settings();
    QCOMPARE(active_stream.stream_name, QStringLiteral("cam-2"));
    QCOMPARE(active_stream.labels_enabled, false);
    QCOMPARE(active_stream.algorithm_id, QStringLiteral("contour_mask"));
    QCOMPARE(active_stream.algorithm_preset, QStringLiteral("mask_heavy"));
    QCOMPARE(active_stream.algorithm_overlay_enabled, true);
    QVERIFY(active_stream.manual_processing_policy_enabled);
    QCOMPARE(active_stream.manual_display_fps, 19);
    QCOMPARE(active_stream.manual_backend_fps, 8);
    QCOMPARE(active_stream.manual_processing_pixels, 640 * 360);

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
    auto* operator_profile_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_operator_profile_combo")
    );
    auto* manual_processing_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_active_manual_processing_checkbox")
    );
    auto* manual_display_fps_spin = panel.findChild<QSpinBox*>(
        QStringLiteral("settings_active_display_fps_spin")
    );
    auto* manual_backend_fps_spin = panel.findChild<QSpinBox*>(
        QStringLiteral("settings_active_backend_fps_spin")
    );
    auto* manual_processing_pixels_spin = panel.findChild<QSpinBox*>(
        QStringLiteral("settings_active_processing_pixels_spin")
    );

    QVERIFY(active_stream_combo != nullptr);
    QVERIFY(active_algorithm_combo != nullptr);
    QVERIFY(active_labels_checkbox != nullptr);
    QVERIFY(operator_profile_combo != nullptr);
    QVERIFY(manual_processing_checkbox != nullptr);
    QVERIFY(manual_display_fps_spin != nullptr);
    QVERIFY(manual_backend_fps_spin != nullptr);
    QVERIFY(manual_processing_pixels_spin != nullptr);

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
        QVERIFY(!settings_value.manual_processing_policy_enabled);
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
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("balanced"));
    }

    const int debug_heavy_index
        = operator_profile_combo->findData(QStringLiteral("debug_heavy"));
    QVERIFY(debug_heavy_index >= 0);
    operator_profile_combo->setCurrentIndex(debug_heavy_index);

    QCOMPARE(stream_settings_spy.count(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const stream_settings settings_value
            = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(
            settings_value.algorithm_id, QStringLiteral("contour_mask")
        );
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("mask_heavy"));
        QVERIFY(settings_value.algorithm_overlay_enabled);
    }

    manual_processing_checkbox->setChecked(true);
    manual_display_fps_spin->setValue(20);
    manual_backend_fps_spin->setValue(10);
    manual_processing_pixels_spin->setValue(512 * 512);

    QVERIFY(stream_settings_spy.count() >= 3);
    {
        const auto args = stream_settings_spy.takeLast();
        const stream_settings settings_value
            = qvariant_cast<stream_settings>(args.at(0));
        QVERIFY(settings_value.manual_processing_policy_enabled);
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("mask_heavy"));
        QVERIFY(settings_value.algorithm_overlay_enabled);
        QCOMPARE(settings_value.manual_display_fps, 20);
        QCOMPARE(settings_value.manual_backend_fps, 10);
        QCOMPARE(settings_value.manual_processing_pixels, 512 * 512);
    }
    stream_settings_spy.clear();

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
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("mask_heavy"));
        QVERIFY(settings_value.algorithm_overlay_enabled);
        QVERIFY(settings_value.manual_processing_policy_enabled);
        QCOMPARE(settings_value.manual_display_fps, 20);
    }
}

void main_window_tests::settings_panel_emits_log_mode_changes() {
    settings_panel panel;

    QSignalSpy log_mode_spy(&panel, &settings_panel::log_mode_changed);
    auto* log_mode_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_log_mode_combo")
    );

    QVERIFY(log_mode_combo != nullptr);
    QCOMPARE(panel.log_mode(), frontend_log_mode::release);

    log_mode_combo->setCurrentIndex(1);

    QCOMPARE(log_mode_spy.count(), 1);
    QCOMPARE(
        log_mode_spy.takeFirst().at(0).value<frontend_log_mode>(),
        frontend_log_mode::debug
    );
    QCOMPARE(panel.log_mode(), frontend_log_mode::debug);

    log_mode_combo->setCurrentIndex(0);

    QCOMPARE(log_mode_spy.count(), 1);
    QCOMPARE(
        log_mode_spy.takeFirst().at(0).value<frontend_log_mode>(),
        frontend_log_mode::release
    );
    QCOMPARE(panel.log_mode(), frontend_log_mode::release);
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
    auto* operator_profile_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_operator_profile_combo")
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
    QVERIFY(operator_profile_combo != nullptr);
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
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("balanced"));
        QCOMPARE(settings_value.algorithm_overlay_enabled, false);
    }

    QCOMPARE(
        preset_combo->currentData().toString(), QStringLiteral("balanced")
    );
    QCOMPARE(operator_profile_combo->currentData().toString(), QStringLiteral("balanced"));
    QVERIFY(summary_label->text().contains(QStringLiteral("contours and masks")));

    const int debug_heavy_index
        = operator_profile_combo->findData(QStringLiteral("debug_heavy"));
    QVERIFY(debug_heavy_index >= 0);
    operator_profile_combo->setCurrentIndex(debug_heavy_index);
    QCOMPARE(stream_settings_spy.count(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const stream_settings settings_value
            = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(
            settings_value.algorithm_id, QStringLiteral("contour_mask")
        );
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("mask_heavy"));
        QCOMPARE(settings_value.algorithm_overlay_enabled, true);
    }
    QCOMPARE(
        preset_combo->currentData().toString(), QStringLiteral("mask_heavy")
    );

    overlay_checkbox->setChecked(false);

    QCOMPARE(stream_settings_spy.count(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const stream_settings settings_value
            = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("mask_heavy"));
        QCOMPARE(settings_value.algorithm_overlay_enabled, false);
    }
    QCOMPARE(operator_profile_combo->currentData().toString(), QStringLiteral("custom"));
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
            timestamp, frontend_log_area::active, frontend_log_severity::info,
            QStringLiteral("stream_settings"),
            QStringLiteral("algorithm preference updated"),
            QStringLiteral("cam-1"),
            QStringLiteral("backend runtime uses contour_mask"),
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
            .manual_processing_policy_enabled = true,
            .manual_display_fps = 17,
            .manual_backend_fps = 9,
            .manual_processing_pixels = 320 * 240,
        }
    );

    const stream_settings settings_value = cell.current_stream_settings();
    QCOMPARE(settings_value.stream_name, QStringLiteral("cam-7"));
    QCOMPARE(settings_value.algorithm_id, QStringLiteral("contour_mask"));
    QCOMPARE(settings_value.algorithm_preset, QStringLiteral("mask_heavy"));
    QCOMPARE(settings_value.algorithm_overlay_enabled, true);
    QVERIFY(settings_value.manual_processing_policy_enabled);
    QCOMPARE(settings_value.manual_display_fps, 17);
    QCOMPARE(settings_value.manual_backend_fps, 9);
    QCOMPARE(settings_value.manual_processing_pixels, 320 * 240);

    cell.set_log_mode(frontend_log_mode::debug);
    QCOMPARE(cell.current_log_mode(), frontend_log_mode::debug);

    cell.set_runtime_metrics(
        stream_runtime_metrics {
            .input_fps = 29.7,
            .backend_fps = 9.8,
            .input_width = 1280,
            .input_height = 720,
            .processed_width = 320,
            .processed_height = 180,
            .effective_display_fps = 17,
            .effective_backend_fps = 9,
            .effective_processing_pixels = 320 * 180,
            .manual_policy_active = true,
        }
    );
    const stream_runtime_metrics metrics = cell.current_runtime_metrics();
    QCOMPARE(metrics.processed_width, 320);
    QCOMPARE(metrics.processed_height, 180);
    QCOMPARE(metrics.effective_backend_fps, 9);
    QVERIFY(metrics.manual_policy_active);
}

void main_window_tests::stream_cell_status_badges_render_different_modes() {
    const stream_settings balanced_status {
        .stream_name = QStringLiteral("cam-status"),
        .labels_enabled = true,
        .algorithm_id = QStringLiteral("motion_baseline"),
        .algorithm_preset = QStringLiteral("balanced"),
        .algorithm_overlay_enabled = false,
    };
    const stream_settings debug_heavy_status {
        .stream_name = QStringLiteral("cam-status"),
        .labels_enabled = true,
        .algorithm_id = QStringLiteral("contour_mask"),
        .algorithm_preset = QStringLiteral("mask_heavy"),
        .algorithm_overlay_enabled = true,
    };

    const QImage release_status
        = main_window_tests_support::render_stream_cell_status_region(
            balanced_status, frontend_log_mode::release
        );
    const QImage debug_status
        = main_window_tests_support::render_stream_cell_status_region(
            balanced_status, frontend_log_mode::debug
        );
    const QImage debug_heavy_algorithm_status
        = main_window_tests_support::render_stream_cell_status_region(
            debug_heavy_status, frontend_log_mode::debug
        );

    QVERIFY(
        main_window_tests_support::differing_pixel_count(
            release_status, debug_status
        )
            > 0
    );
    QVERIFY(
        main_window_tests_support::differing_pixel_count(
            debug_status, debug_heavy_algorithm_status
        )
            > 0
    );
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
