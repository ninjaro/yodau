#include "shell/active_edit_controller.hpp"
#include "shell/active_edit_session.hpp"
#include "shell/active_editor_bridge.hpp"
#include "shell/app_settings.hpp"
#include "shell/stream_widget_bridge.hpp"
#include "ui/main_window_tests.hpp"

#include "widgets/grid_view.hpp"
#include "widgets/settings_panel.hpp"
#include "widgets/stream_board.hpp"
#include "widgets/stream_cell.hpp"

#include <QComboBox>
#include <QtTest/QtTest>

// QtTest discovers these private slots through the meta-object.
// NOLINTBEGIN(readability-convert-member-functions-to-static)

void main_window_tests::editor_bridge_projection_and_preview() {
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
    QCOMPARE(
        static_cast<int>(board.active_cell()->draft_points_pct().size()), 2
    );
    auto* active_stream_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_stream_combo")
    );
    QVERIFY(active_stream_combo != nullptr);
    QCOMPARE(active_stream_combo->currentText(), QStringLiteral("cam-1"));

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
    QCOMPARE(panel.current_active_template_settings().template_name, QString());

    editor_bridge.apply_active_stream(
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

void main_window_tests::edit_controller_sync_and_reset() {
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
    widget_bridge.apply_active_stream(
        QStringLiteral("cam-1"), active_settings, edit_session
    );
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
    QCOMPARE(
        static_cast<int>(board.active_cell()->draft_points_pct().size()), 2
    );

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
    QCOMPARE(
        static_cast<int>(board.active_cell()->draft_points_pct().size()), 2
    );

    edit_controller.reset_after_template_applied();
    QCOMPARE(panel.current_active_template_settings().template_name, QString());
    QCOMPARE(
        static_cast<int>(board.active_cell()->draft_points_pct().size()), 0
    );
}

void main_window_tests::edit_session_draft_templates_and_lines() {
    active_edit_session session;

    session.set_draft_line_profile(
        line_profile {
            .name = QStringLiteral(" north "),
            .color = QColor(Qt::green),
            .color_mode_id = QStringLiteral("manual"),
            .closed = true,
            .width_text = QStringLiteral("String Heavy"),
            .length_text = QStringLiteral("long"),
            .response_text = QStringLiteral("resonant"),
        }
    );

    const line_profile& draft_profile = session.draft_line_profile();
    QCOMPARE(draft_profile.name, QStringLiteral("north"));
    QCOMPARE(draft_profile.color_mode_id, QStringLiteral("manual"));
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
    QCOMPARE(saved_line.color_mode_id, QStringLiteral("manual"));
    QVERIFY(saved_line.enabled);
    QVERIFY(session.has_template(QStringLiteral("north")));
    QCOMPARE(
        static_cast<int>(session.stream_lines(QStringLiteral("cam-1")).size()),
        1
    );
    QVERIFY(session.set_stream_line_enabled(
        QStringLiteral("cam-1"), QStringLiteral("north"), false
    ));
    const auto disabled_line = session.find_stream_line(
        QStringLiteral("cam-1"), QStringLiteral("north")
    );
    QVERIFY(disabled_line.has_value());
    QVERIFY(!disabled_line->enabled);

    template_apply_settings resolved_settings
        = session.resolved_template_settings(
            template_apply_settings {
                .template_name = QStringLiteral("north"),
                .color = QColor(Qt::yellow),
                .color_mode_id = QStringLiteral("negative"),
                .width_text = QStringLiteral("thin"),
                .length_text = QStringLiteral("short"),
                .response_text = QStringLiteral("dry"),
            },
            true
        );
    QCOMPARE(resolved_settings.color_mode_id, QStringLiteral("negative_auto"));
    QCOMPARE(resolved_settings.width_text, QStringLiteral("string_heavy"));
    QCOMPARE(resolved_settings.length_text, QStringLiteral("long"));
    QCOMPARE(resolved_settings.response_text, QStringLiteral("resonant"));

    session.set_active_template_settings(resolved_settings);
    const stream_cell::line_instance applied_line
        = session.store_applied_template_line(
            QStringLiteral("cam-1"), session.active_template_settings()
        );

    QCOMPARE(applied_line.template_name, QStringLiteral("north"));
    QCOMPARE(applied_line.color_mode_id, QStringLiteral("negative_auto"));
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

// NOLINTEND(readability-convert-member-functions-to-static)
