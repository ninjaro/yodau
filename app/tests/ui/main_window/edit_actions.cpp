#include "shell/active_edit_actions.hpp"
#include "shell/active_edit_controller.hpp"
#include "shell/active_edit_session.hpp"
#include "shell/active_edit_workflow.hpp"
#include "shell/app_settings.hpp"
#include "shell/stream_catalog_state.hpp"
#include "shell/stream_route_state.hpp"
#include "shell/stream_widget_bridge.hpp"
#include "streams/stream_manager.hpp"
#include "ui/main_window_tests.hpp"
#include "widgets/settings_panel.hpp"
#include "widgets/stream_board.hpp"

#include <QtTest/QtTest>

// QtTest discovers these private slots through the meta-object.
// NOLINTBEGIN(readability-convert-member-functions-to-static)

void main_window_tests::actions_apply_lines_and_templates() {
    settings_panel panel;
    stream_board board;
    stream_widget_bridge widget_bridge(&board, &panel);
    active_edit_session edit_session;
    active_edit_controller edit_controller(edit_session, widget_bridge);
    yodau::core::stream_manager stream_mgr;
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
    widget_bridge.apply_active_stream(
        QStringLiteral("cam-1"), active_settings, edit_session
    );
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
    QVERIFY(
        line_result.points_text.contains(QStringLiteral("(10.000,10.000)"))
    );
    QCOMPARE(static_cast<int>(stream_mgr.stream_lines("cam-1").size()), 1);
    QCOMPARE(
        QString::fromStdString(stream_mgr.stream_lines("cam-1").front()),
        QStringLiteral("north")
    );
    QCOMPARE(
        static_cast<int>(
            edit_session.stream_lines(QStringLiteral("cam-1")).size()
        ),
        1
    );
    QCOMPARE(
        edit_session.stream_lines(QStringLiteral("cam-1"))
            .front()
            .template_name,
        QStringLiteral("north")
    );
    const auto stored_core_profile = stream_mgr.find_line_profile("north");
    QVERIFY(stored_core_profile.has_value());
    QVERIFY(std::fabs(stored_core_profile->visual_width - 6.5f) < 0.01f);
    QVERIFY(std::fabs(stored_core_profile->interaction_width - 8.45f) < 0.01f);
    QVERIFY(std::fabs(stored_core_profile->effective_length - 1.35f) < 0.01f);
    QVERIFY(std::fabs(stored_core_profile->damping - 0.8f) < 0.01f);

    const auto attached_core_profile
        = stream_mgr.find_stream_line_profile("cam-1", "north");
    QVERIFY(attached_core_profile.has_value());
    QVERIFY(
        std::fabs(
            attached_core_profile->visual_width
            - stored_core_profile->visual_width
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
    QCOMPARE(
        static_cast<int>(
            edit_session.stream_lines(QStringLiteral("cam-1")).size()
        ),
        2
    );
    QCOMPARE(panel.current_active_template_settings().template_name, QString());
    const auto global_profile_after_apply
        = stream_mgr.find_line_profile("north");
    QVERIFY(global_profile_after_apply.has_value());
    QVERIFY(
        std::fabs(
            global_profile_after_apply->visual_width
            - stored_core_profile->visual_width
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
        active_edit_actions::line_save_status::core_error
    );
    QCOMPARE(
        static_cast<int>(
            edit_session.stream_lines(QStringLiteral("cam-1")).size()
        ),
        before_line_count
    );
    QCOMPARE(
        static_cast<int>(
            edit_session.stream_lines(QStringLiteral("missing-stream")).size()
        ),
        0
    );
    QCOMPARE(static_cast<int>(active_cell->draft_points_pct().size()), 2);
}

void main_window_tests::actions_save_variant_detaches_source() {
    settings_panel panel;
    stream_board board;
    stream_widget_bridge widget_bridge(&board, &panel);
    active_edit_session edit_session;
    active_edit_controller edit_controller(edit_session, widget_bridge);
    yodau::core::stream_manager stream_mgr;
    active_edit_actions edit_actions(
        &stream_mgr, edit_session, widget_bridge, edit_controller
    );

    stream_mgr.add_stream("/tmp/cam-edit.mp4", "cam-edit", "file", true);

    const stream_settings active_settings {
        .stream_name = QStringLiteral("cam-edit"),
        .labels_enabled = true,
        .algorithm_id = QStringLiteral("motion_baseline"),
        .algorithm_preset = QStringLiteral("balanced"),
        .algorithm_overlay_enabled = false,
    };
    auto* grid_tile = widget_bridge.show_stream_in_grid(
        QStringLiteral("cam-edit"), active_settings, edit_session,
        stream_widget_bridge::grid_stream_binding {
            .path = QStringLiteral("/tmp/cam-edit.mp4"),
            .type = QStringLiteral("file"),
            .loop = true,
        }
    );
    QVERIFY(grid_tile != nullptr);
    widget_bridge.apply_active_stream(
        QStringLiteral("cam-edit"), active_settings, edit_session
    );
    widget_bridge.sync_active_persistent(
        QStringLiteral("cam-edit"), edit_session
    );

    auto* active_cell = board.active_cell();
    QVERIFY(active_cell != nullptr);
    active_cell->set_draft_points_pct(
        {
            QPointF(10.0, 10.0),
            QPointF(55.0, 20.0),
            QPointF(80.0, 70.0),
        }
    );

    const auto initial_line = edit_actions.save_active_line(
        QStringLiteral("cam-edit"),
        line_profile {
            .name = QStringLiteral("north"),
            .color = QColor(Qt::green),
            .closed = true,
            .width_text = QStringLiteral("string_heavy"),
            .length_text = QStringLiteral("long"),
            .response_text = QStringLiteral("resonant"),
        },
        *active_cell
    );
    QCOMPARE(initial_line.status, active_edit_actions::line_save_status::saved);
    QCOMPARE(static_cast<int>(stream_mgr.stream_lines("cam-edit").size()), 1);
    QCOMPARE(
        QString::fromStdString(stream_mgr.stream_lines("cam-edit").front()),
        QStringLiteral("north")
    );

    const line_edit_request preview_request {
        .stream_name = QStringLiteral("cam-edit"),
        .source_line_name = QStringLiteral("north"),
        .profile = line_profile {
            .name = QStringLiteral("north_alt"),
            .color = QColor(Qt::green),
            .closed = true,
            .width_text = QStringLiteral("string_heavy"),
            .length_text = QStringLiteral("long"),
            .response_text = QStringLiteral("resonant"),
        },
        .points_pct = {
            QPointF(10.0, 10.0),
            QPointF(45.0, 30.0),
            QPointF(80.0, 70.0),
        },
    };
    const line_edit_request applied_preview
        = edit_controller.apply_line_edit_preview(preview_request);
    QCOMPARE(applied_preview.source_line_name, QStringLiteral("north"));
    QVERIFY(active_cell->has_line_edit_preview());
    QCOMPARE(
        active_cell->line_edit_preview_name(), QStringLiteral("north_alt")
    );
    QVERIFY(!active_cell->is_drawing_enabled());

    const auto line_edit_result = edit_actions.save_active_line_edit(
        QStringLiteral("cam-edit"), preview_request
    );

    QCOMPARE(
        line_edit_result.status,
        active_edit_actions::line_edit_save_status::saved
    );
    QCOMPARE(
        line_edit_result.request.source_line_name, QStringLiteral("north")
    );
    QCOMPARE(line_edit_result.final_name, QStringLiteral("north_alt"));
    QCOMPARE(line_edit_result.point_count, 3);
    QCOMPARE(static_cast<int>(stream_mgr.stream_lines("cam-edit").size()), 1);
    QCOMPARE(
        QString::fromStdString(stream_mgr.stream_lines("cam-edit").front()),
        QStringLiteral("north_alt")
    );
    QCOMPARE(
        static_cast<int>(
            edit_session.stream_lines(QStringLiteral("cam-edit")).size()
        ),
        1
    );
    QCOMPARE(
        edit_session.stream_lines(QStringLiteral("cam-edit"))
            .front()
            .template_name,
        QStringLiteral("north_alt")
    );
    QVERIFY(edit_session.has_template(QStringLiteral("north")));
    QVERIFY(edit_session.has_template(QStringLiteral("north_alt")));
    QVERIFY(!active_cell->has_line_edit_preview());
}

void main_window_tests::actions_detach_preserves_template() {
    settings_panel panel;
    stream_board board;
    stream_widget_bridge widget_bridge(&board, &panel);
    active_edit_session edit_session;
    active_edit_controller edit_controller(edit_session, widget_bridge);
    yodau::core::stream_manager stream_mgr;
    active_edit_actions edit_actions(
        &stream_mgr, edit_session, widget_bridge, edit_controller
    );

    stream_mgr.add_stream("/tmp/cam-detach.mp4", "cam-detach", "file", true);

    const stream_settings active_settings {
        .stream_name = QStringLiteral("cam-detach"),
        .labels_enabled = true,
        .algorithm_id = QStringLiteral("motion_baseline"),
        .algorithm_preset = QStringLiteral("balanced"),
        .algorithm_overlay_enabled = false,
    };
    auto* grid_tile = widget_bridge.show_stream_in_grid(
        QStringLiteral("cam-detach"), active_settings, edit_session,
        stream_widget_bridge::grid_stream_binding {
            .path = QStringLiteral("/tmp/cam-detach.mp4"),
            .type = QStringLiteral("file"),
            .loop = true,
        }
    );
    QVERIFY(grid_tile != nullptr);
    widget_bridge.apply_active_stream(
        QStringLiteral("cam-detach"), active_settings, edit_session
    );
    widget_bridge.sync_active_persistent(
        QStringLiteral("cam-detach"), edit_session
    );

    auto* active_cell = board.active_cell();
    QVERIFY(active_cell != nullptr);
    active_cell->set_draft_points_pct(
        {
            QPointF(15.0, 20.0),
            QPointF(70.0, 35.0),
        }
    );

    const auto initial_line = edit_actions.save_active_line(
        QStringLiteral("cam-detach"),
        line_profile {
            .name = QStringLiteral("north"),
            .color = QColor(Qt::green),
            .closed = false,
            .width_text = QStringLiteral("string_heavy"),
            .length_text = QStringLiteral("long"),
            .response_text = QStringLiteral("resonant"),
        },
        *active_cell
    );
    QCOMPARE(initial_line.status, active_edit_actions::line_save_status::saved);
    QCOMPARE(static_cast<int>(stream_mgr.stream_lines("cam-detach").size()), 1);
    QCOMPARE(
        static_cast<int>(
            edit_session.stream_lines(QStringLiteral("cam-detach")).size()
        ),
        1
    );
    QVERIFY(edit_session.has_template(QStringLiteral("north")));

    const auto detach_result = edit_actions.detach_stream_line(
        QStringLiteral("cam-detach"), QStringLiteral("north")
    );

    QCOMPARE(
        detach_result.status, active_edit_actions::line_detach_status::detached
    );
    QCOMPARE(detach_result.line_name, QStringLiteral("north"));
    QCOMPARE(static_cast<int>(stream_mgr.stream_lines("cam-detach").size()), 0);
    QCOMPARE(
        static_cast<int>(
            edit_session.stream_lines(QStringLiteral("cam-detach")).size()
        ),
        0
    );
    QVERIFY(edit_session.has_template(QStringLiteral("north")));
}

void main_window_tests::edit_workflow_logs_and_follow_up() {
    settings_panel panel;
    stream_board board;
    stream_widget_bridge widget_bridge(&board, &panel);
    active_edit_session edit_session;
    stream_catalog_state catalog_state;
    stream_route_state route_state;
    active_edit_controller edit_controller(edit_session, widget_bridge);
    yodau::core::stream_manager stream_mgr;
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
    widget_bridge.apply_active_stream(
        QStringLiteral("cam-1"), active_settings, edit_session
    );
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
        mode_result.entries.front().algorithm_id, QStringLiteral("spot_grid")
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
    QVERIFY(line_profile_result.entries.front().detail.contains(
        QStringLiteral("name=north")
    ));
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
        line_save_result.entries.back().message, QStringLiteral("line added")
    );
    QVERIFY(line_save_result.refresh_fps);
    QVERIFY(line_save_result.entries.back().detail.contains(
        QStringLiteral("template=north")
    ));
    QCOMPARE(
        line_save_result.entries.back().algorithm_id,
        QStringLiteral("spot_grid")
    );

    const auto detach_result
        = workflow.detach_active_line(QStringLiteral("north"));
    QCOMPARE(detach_result.entries.size(), 1);
    QCOMPARE(
        detach_result.entries.front().message,
        QStringLiteral("line detached from stream")
    );
    QVERIFY(detach_result.entries.front().detail.contains(
        QStringLiteral("line=north")
    ));
    QVERIFY(detach_result.refresh_fps);
    QCOMPARE(
        static_cast<int>(
            edit_session.stream_lines(QStringLiteral("cam-1")).size()
        ),
        0
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
    QVERIFY(template_settings_result.entries.front().detail.contains(
        QStringLiteral("template=north")
    ));

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
    QCOMPARE(
        template_apply_result.entries.front().algorithm_id,
        QStringLiteral("spot_grid")
    );
}

// NOLINTEND(readability-convert-member-functions-to-static)
