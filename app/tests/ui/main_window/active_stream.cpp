#include "shell/active_edit_session.hpp"
#include "shell/active_stream_state.hpp"
#include "shell/active_stream_workflow.hpp"
#include "shell/app_settings.hpp"
#include "shell/stream_catalog_state.hpp"
#include "shell/stream_route_state.hpp"
#include "shell/stream_widget_bridge.hpp"
#include "ui/main_window_tests.hpp"
#include "widgets/settings_panel.hpp"
#include "widgets/stream_board.hpp"

#include <QComboBox>
#include <QtTest/QtTest>

// QtTest discovers these private slots through the meta-object.
// NOLINTBEGIN(readability-convert-member-functions-to-static)

void main_window_tests::active_stream_selection_and_settings() {
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

    const auto selected_cam2
        = active_streams.set_active_stream(QStringLiteral("cam-2"));
    QCOMPARE(selected_cam2.active_name, QStringLiteral("cam-2"));
    QCOMPARE(route_state.active_stream_name(), QStringLiteral("cam-2"));
    QVERIFY(board.active_cell() != nullptr);
    QCOMPARE(board.active_cell()->get_name(), QStringLiteral("cam-2"));

    const auto updated_cam2 = active_streams.apply_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral(" cam-2 "),
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
    QCOMPARE(
        updated_cam2.settings.algorithm_id, QStringLiteral("contour_mask")
    );
    QCOMPARE(
        updated_cam2.settings.algorithm_preset, QStringLiteral("mask_heavy")
    );
    QVERIFY(updated_cam2.settings.algorithm_overlay_enabled);
    QCOMPARE(
        catalog_state.settings_for(QStringLiteral("cam-2")).algorithm_id,
        QStringLiteral("contour_mask")
    );
    QCOMPARE(
        board.active_cell()->current_stream_settings().stream_name,
        QStringLiteral("cam-2")
    );
    QCOMPARE(
        board.active_cell()->current_stream_settings().algorithm_id,
        QStringLiteral("contour_mask")
    );

    const auto preset_update = active_streams.apply_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-2"),
            .labels_enabled = false,
            .algorithm_id = QStringLiteral("contour mask"),
            .algorithm_preset = QStringLiteral("balanced"),
            .movement_display_mode = QStringLiteral("off"),
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
    QCOMPARE(
        preset_update.settings.algorithm_id, QStringLiteral("contour_mask")
    );
    QCOMPARE(
        preset_update.settings.algorithm_preset, QStringLiteral("balanced")
    );
    QVERIFY(!preset_update.settings.algorithm_overlay_enabled);

    const auto processing_update = active_streams.apply_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-2"),
            .labels_enabled = false,
            .algorithm_id = QStringLiteral("contour mask"),
            .algorithm_preset = QStringLiteral("balanced"),
            .movement_display_mode = QStringLiteral("off"),
            .algorithm_overlay_enabled = false,
            .manual_processing_policy_enabled = true,
            .manual_display_fps = 21,
            .manual_core_fps = 8,
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
    QCOMPARE(processing_update.settings.manual_core_fps, 8);
    QCOMPARE(processing_update.settings.manual_processing_pixels, 640 * 360);

    const auto updated_cam1 = active_streams.apply_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-1"),
            .labels_enabled = false,
            .algorithm_id = QStringLiteral("spot_grid"),
            .algorithm_preset = QStringLiteral("dense"),
            .algorithm_overlay_enabled = true,
        }
    );

    QCOMPARE(
        updated_cam1.outcome_value,
        active_stream_state::settings_result::outcome::updated
    );
    QCOMPARE(updated_cam1.active_name, QStringLiteral("cam-2"));
    QCOMPARE(route_state.active_stream_name(), QStringLiteral("cam-2"));
    QVERIFY(board.active_cell() != nullptr);
    QCOMPARE(board.active_cell()->get_name(), QStringLiteral("cam-2"));
    QCOMPARE(
        catalog_state.settings_for(QStringLiteral("cam-1")).algorithm_id,
        QStringLiteral("spot_grid")
    );

    const auto switched_to_cam1
        = active_streams.set_active_stream(QStringLiteral("cam-1"));
    QCOMPARE(switched_to_cam1.active_name, QStringLiteral("cam-1"));
    QCOMPARE(route_state.active_stream_name(), QStringLiteral("cam-1"));
    QVERIFY(board.active_cell() != nullptr);
    QCOMPARE(board.active_cell()->get_name(), QStringLiteral("cam-1"));

    const auto cleared = active_streams.set_active_stream(QString());
    QCOMPARE(cleared.active_name, QString());
    QVERIFY(cleared.changed);
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

void main_window_tests::stream_workflow_logs_and_follow_up() {
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

    const auto selected_cam2
        = workflow.set_active_stream(QStringLiteral("cam-2"));
    QCOMPARE(selected_cam2.entries.size(), 1);
    QCOMPARE(
        selected_cam2.entries.front().message,
        QStringLiteral("active stream selected")
    );
    QCOMPARE(
        selected_cam2.entries.front().stream_name, QStringLiteral("cam-2")
    );
    QCOMPARE(
        selected_cam2.entries.front().algorithm_id,
        QStringLiteral("contour_mask")
    );
    QVERIFY(selected_cam2.refresh_fps);
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
    QCOMPARE(updated_cam2.entries.front().stream_name, QStringLiteral("cam-2"));
    QVERIFY(!updated_cam2.refresh_fps);

    const auto preset_update = workflow.apply_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-2"),
            .labels_enabled = false,
            .algorithm_id = QStringLiteral("contour mask"),
            .algorithm_preset = QStringLiteral("balanced"),
            .movement_display_mode = QStringLiteral("off"),
            .algorithm_overlay_enabled = false,
        }
    );

    QCOMPARE(preset_update.entries.size(), 1);
    QCOMPARE(
        preset_update.entries.front().message,
        QStringLiteral("algorithm preference updated")
    );
    QVERIFY(preset_update.entries.front().detail.contains(
        QStringLiteral("core runtime uses contour_mask")
    ));
    QVERIFY(preset_update.entries.front().detail.contains(
        QStringLiteral("preset=balanced")
    ));
    QVERIFY(preset_update.entries.front().detail.contains(
        QStringLiteral("movement_display=off")
    ));
    QVERIFY(!preset_update.refresh_fps);

    const auto algorithm_update = workflow.apply_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-2"),
            .labels_enabled = false,
            .algorithm_id = QStringLiteral("spot_grid"),
            .algorithm_preset = QStringLiteral("dense"),
            .movement_display_mode = QStringLiteral("off"),
            .algorithm_overlay_enabled = false,
        }
    );

    QCOMPARE(algorithm_update.entries.size(), 1);
    QCOMPARE(
        algorithm_update.entries.front().message,
        QStringLiteral("algorithm preference updated")
    );
    QCOMPARE(algorithm_update.entries.front().severity, app_log_severity::info);
    QVERIFY(algorithm_update.entries.front().detail.contains(
        QStringLiteral("core runtime uses spot_grid")
    ));
    QVERIFY(algorithm_update.entries.front().detail.contains(
        QStringLiteral("preset=dense")
    ));
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
            .movement_display_mode = QStringLiteral("off"),
            .algorithm_overlay_enabled = false,
            .manual_processing_policy_enabled = true,
            .manual_display_fps = 18,
            .manual_core_fps = 9,
            .manual_processing_pixels = 320 * 180,
        }
    );

    QCOMPARE(processing_update.entries.size(), 1);
    QCOMPARE(
        processing_update.entries.front().message,
        QStringLiteral("processing tuning updated")
    );
    QVERIFY(processing_update.entries.front().detail.contains(
        QStringLiteral("display_fps=18")
    ));
    QVERIFY(processing_update.refresh_fps);

    const auto updated_cam1 = workflow.apply_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-1"),
            .labels_enabled = false,
            .algorithm_id = QStringLiteral("spot_grid"),
            .algorithm_preset = QStringLiteral("dense"),
            .algorithm_overlay_enabled = true,
        }
    );

    QCOMPARE(updated_cam1.entries.size(), 2);
    QCOMPARE(
        updated_cam1.entries.front().message,
        QStringLiteral("line labels disabled")
    );
    QCOMPARE(updated_cam1.entries.back().stream_name, QStringLiteral("cam-1"));
    QCOMPARE(
        updated_cam1.entries.back().message,
        QStringLiteral("algorithm preference updated")
    );
    QVERIFY(!updated_cam1.refresh_fps);
    QCOMPARE(route_state.active_stream_name(), QStringLiteral("cam-2"));
    QVERIFY(board.active_cell() != nullptr);
    QCOMPARE(board.active_cell()->get_name(), QStringLiteral("cam-2"));

    const auto ignored_empty
        = workflow.apply_stream_settings(stream_settings {});
    QCOMPARE(ignored_empty.entries.size(), 0);
    QVERIFY(!ignored_empty.refresh_fps);
}

// NOLINTEND(readability-convert-member-functions-to-static)
