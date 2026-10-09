#include "shell/active_edit_session.hpp"
#include "shell/app_settings.hpp"
#include "shell/stream_route_state.hpp"
#include "shell/stream_widget_bridge.hpp"
#include "ui/main_window_tests.hpp"

#include "widgets/grid_view.hpp"
#include "widgets/settings_panel.hpp"
#include "widgets/stream_board.hpp"

#include <QComboBox>
#include <QTreeWidget>
#include <QtTest/QtTest>

// QtTest discovers these private slots through the meta-object.
// NOLINTBEGIN(readability-convert-member-functions-to-static)

void main_window_tests::bridge_active_state_and_template_preview() {
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
        widget_bridge.tile_for_stream_name(
            QStringLiteral("cam-1"), route_state
        ),
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
        panel
            .findChild<QComboBox*>(
                QStringLiteral("settings_active_stream_combo")
            )
            ->currentText(),
        QStringLiteral("cam-1")
    );

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

    auto* grid_cell
        = board.grid_mode()->peek_stream_cell(QStringLiteral("cam-2"));
    QVERIFY(grid_cell != nullptr);
    QCOMPARE(
        grid_cell->current_stream_settings().algorithm_id,
        QStringLiteral("spot_grid")
    );

    widget_bridge.apply_active_stream(
        QString(), stream_settings {}, edit_session
    );
    QVERIFY(board.active_cell() == nullptr);
    QVERIFY(
        panel
            .findChild<QComboBox*>(
                QStringLiteral("settings_active_stream_combo")
            )
            ->currentText()
        != QStringLiteral("cam-1")
    );
}

void main_window_tests::bridge_syncs_editor_controls() {
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
    QCOMPARE(panel.current_active_template_settings().template_name, QString());
    QCOMPARE(
        panel.current_active_template_settings().width_text,
        default_line_width_text()
    );
}

void main_window_tests::bridge_routes_grid_visibility() {
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

    auto* streams_list = panel.findChild<QTreeWidget*>(
        QStringLiteral("settings_streams_list")
    );
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
    QCOMPARE(
        panel
            .findChild<QComboBox*>(
                QStringLiteral("settings_active_stream_combo")
            )
            ->currentText(),
        QStringLiteral("cam-2")
    );

    widget_bridge.hide_stream_from_grid(QStringLiteral("cam-1"), false);
    QVERIFY(!board.grid_mode()->has_stream(QStringLiteral("cam-1")));
    QVERIFY(board.active_cell() != nullptr);
    QCOMPARE(board.active_cell()->get_name(), QStringLiteral("cam-2"));

    widget_bridge.hide_stream_from_grid(QStringLiteral("cam-2"), true);
    QVERIFY(board.active_cell() == nullptr);
    QVERIFY(
        panel
            .findChild<QComboBox*>(
                QStringLiteral("settings_active_stream_combo")
            )
            ->currentText()
        != QStringLiteral("cam-2")
    );
}

void main_window_tests::bridge_syncs_log_mode() {
    settings_panel panel;
    panel.set_log_mode(app_log_mode::debug);

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
    QCOMPARE(cam1_tile->current_log_mode(), app_log_mode::debug);
    QCOMPARE(cam2_tile->current_log_mode(), app_log_mode::debug);

    board.set_active_stream(QStringLiteral("cam-2"));
    QVERIFY(board.active_cell() != nullptr);
    QCOMPARE(board.active_cell()->current_log_mode(), app_log_mode::debug);

    panel.set_log_mode(app_log_mode::release);
    widget_bridge.sync_visible_log_mode(panel.log_mode());

    QCOMPARE(cam1_tile->current_log_mode(), app_log_mode::release);
    QCOMPARE(board.active_cell()->current_log_mode(), app_log_mode::release);
}

// NOLINTEND(readability-convert-member-functions-to-static)
