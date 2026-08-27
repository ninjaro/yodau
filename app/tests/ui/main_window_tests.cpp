#include "ui/main_window_tests.hpp"

#include "configuration/line_configuration.hpp"
#include "core/namespace_alias.hpp"
#include "shell/active_edit_actions.hpp"
#include "shell/active_edit_controller.hpp"
#include "shell/active_edit_session.hpp"
#include "shell/active_edit_workflow.hpp"
#include "shell/active_editor_bridge.hpp"
#include "shell/active_stream_state.hpp"
#include "shell/active_stream_workflow.hpp"
#include "shell/app_log.hpp"
#include "shell/app_settings.hpp"
#include "shell/main_window.hpp"
#include "shell/mobile_session_store.hpp"
#include "shell/processing_feedback_state.hpp"
#include "shell/stream_catalog_state.hpp"
#include "shell/stream_catalog_workflow.hpp"
#include "shell/stream_controller.hpp"
#include "shell/stream_route_state.hpp"
#include "shell/stream_widget_bridge.hpp"
#include "shell/window_state_store.hpp"
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

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDockWidget>
#include <QFile>
#include <QGroupBox>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScopeGuard>
#include <QScrollArea>
#include <QSettings>
#include <QSignalSpy>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QToolBar>
#include <QTreeWidget>
#include <QtTest/QtTest>

#include <cmath>
#include <filesystem>
#include <string_view>

#ifdef KC_KDE
#include <KActionCollection>
#endif

namespace main_window_tests_support {

app_log_entry make_log_entry(
    const QDateTime& timestamp, const app_log_area area,
    const app_log_severity severity, const QString& subsystem,
    const QString& message, const QString& stream_name = QString(),
    const QString& detail = QString(), const QString& algorithm_id = QString(),
    const QString& line_name = QString(), const QString& event_type = QString(),
    const QColor& line_color = QColor()
) {
    app_log_entry entry;
    entry.timestamp = timestamp;
    entry.area = area;
    entry.severity = severity;
    entry.subsystem = subsystem;
    entry.stream_name = stream_name;
    entry.line_name = line_name;
    entry.algorithm_id = algorithm_id;
    entry.event_type = event_type;
    entry.message = message;
    entry.detail = detail;
    entry.line_color = line_color;
    return entry;
}

QImage render_stream_cell_event_region(const stream_settings& settings_value) {
    stream_cell cell(QStringLiteral("cam-render"));
    cell.resize(240, 180);
    cell.setStyleSheet(
        QStringLiteral("background-color: black; color: white;")
    );
    cell.set_stream_settings(settings_value);
    cell.add_event(QPointF(25.0, 25.0), QColor(QStringLiteral("#ff8844")));

    QImage image(cell.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    cell.render(&image);

    return image.copy(QRect(16, 12, 88, 68));
}

QImage render_stream_cell_status_region(
    const stream_settings& settings_value, const app_log_mode log_mode
) {
    stream_cell cell(QStringLiteral("cam-status"));
    cell.resize(240, 180);
    cell.setStyleSheet(
        QStringLiteral("background-color: black; color: white;")
    );
    cell.set_stream_settings(settings_value);
    cell.set_log_mode(log_mode);

    QImage image(cell.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    cell.render(&image);

    return image.copy(QRect(96, 118, 136, 54));
}

QImage
render_stream_cell_wave_region(const stream_cell::line_instance& line_value) {
    stream_cell cell(QStringLiteral("cam-wave"));
    cell.resize(240, 180);
    cell.setStyleSheet(
        QStringLiteral("background-color: black; color: white;")
    );
    cell.set_persistent_lines({ line_value });
    cell.highlight_line_at(
        line_value.template_name, QPointF(50.0, 70.0), 1.0, QString(), 1.0
    );

    QImage image(cell.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    cell.render(&image);

    return image.copy(QRect(24, 98, 192, 56));
}

QImage gradient_test_frame(const QSize& size) {
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < size.height(); y += 1) {
        for (int x = 0; x < size.width(); x += 1) {
            image.setPixelColor(
                x, y,
                QColor(
                    std::clamp(x, 0, 255), std::clamp(y, 0, 255),
                    std::clamp((x + y) / 2, 0, 255)
                )
            );
        }
    }
    return image;
}

QImage render_stream_cell_with_frame(
    const QSize& cell_size, const QImage& frame,
    const std::vector<stream_cell::line_instance>& lines
) {
    stream_cell cell(QStringLiteral("cam-render"));
    cell.resize(cell_size);
    cell.setStyleSheet(
        QStringLiteral("background-color: black; color: white;")
    );
    cell.set_persistent_lines(lines);
    const bool invoked = QMetaObject::invokeMethod(
        &cell, "on_frame_changed", Qt::DirectConnection,
        Q_ARG(QVideoFrame, QVideoFrame(frame))
    );
    Q_ASSERT(invoked);
    Q_UNUSED(invoked);

    QImage image(cell.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    cell.render(&image);
    return image;
}

QImage render_stream_cell_with_frame_and_highlights(
    const QSize& cell_size, const QImage& frame,
    const std::vector<stream_cell::line_instance>& lines,
    const std::vector<std::pair<QString, QPointF>>& highlights
) {
    stream_cell cell(QStringLiteral("cam-render"));
    cell.resize(cell_size);
    cell.setStyleSheet(
        QStringLiteral("background-color: black; color: white;")
    );
    cell.set_persistent_lines(lines);
    const bool invoked = QMetaObject::invokeMethod(
        &cell, "on_frame_changed", Qt::DirectConnection,
        Q_ARG(QVideoFrame, QVideoFrame(frame))
    );
    Q_ASSERT(invoked);
    Q_UNUSED(invoked);

    for (const auto& [line_name, pos_pct] : highlights) {
        cell.highlight_line_at(line_name, pos_pct);
    }

    QImage image(cell.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    cell.render(&image);
    return image;
}

bool color_close(
    const QColor& actual, const QColor& expected, const int tolerance = 4
) {
    return std::abs(actual.red() - expected.red()) <= tolerance
        && std::abs(actual.green() - expected.green()) <= tolerance
        && std::abs(actual.blue() - expected.blue()) <= tolerance;
}

bool point_close(
    const QPointF& actual, const QPointF& expected,
    const double tolerance = 0.15
) {
    return std::abs(actual.x() - expected.x()) <= tolerance
        && std::abs(actual.y() - expected.y()) <= tolerance;
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

// QtTest discovers these private slots as instance methods through the
// meta-object. NOLINTBEGIN(readability-convert-member-functions-to-static)

void main_window_tests::
    active_actions_apply_lines_and_templates_against_core() {
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

void main_window_tests::active_actions_detach_old_line_when_saving_variant() {
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

void main_window_tests::
    active_edit_actions_detach_stream_line_preserves_template() {
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

void main_window_tests::active_edit_tracks_logs_and_follow_up_actions() {
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
    QVERIFY(line_save_result.update_monitor_inventory);
    QCOMPARE(line_save_result.monitor_marker, QStringLiteral("line_added"));
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
    QCOMPARE(detach_result.monitor_marker, QStringLiteral("line_detached"));
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
    QVERIFY(!template_apply_result.update_monitor_inventory);
    QVERIFY(template_apply_result.monitor_marker.isEmpty());
    QCOMPARE(
        template_apply_result.entries.front().algorithm_id,
        QStringLiteral("spot_grid")
    );
}

void main_window_tests::
    active_editor_bridge_tracks_editor_projection_and_preview() {
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

void main_window_tests::
    active_stream_state_tracks_selection_and_settings_application() {
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

void main_window_tests::active_stream_tracks_selection_logs_and_follow_up() {
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
    QVERIFY(selected_cam2.update_monitor_inventory);
    QCOMPARE(
        selected_cam2.monitor_marker, QStringLiteral("active_stream_selected")
    );
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
    QVERIFY(!updated_cam2.update_monitor_inventory);

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
    QVERIFY(!processing_update.update_monitor_inventory);

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
    QVERIFY(!updated_cam1.update_monitor_inventory);
    QVERIFY(updated_cam1.monitor_marker.isEmpty());
    QCOMPARE(route_state.active_stream_name(), QStringLiteral("cam-2"));
    QVERIFY(board.active_cell() != nullptr);
    QCOMPARE(board.active_cell()->get_name(), QStringLiteral("cam-2"));

    const auto ignored_empty
        = workflow.apply_stream_settings(stream_settings {});
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

void main_window_tests::active_edit_session_tracks_draft_templates_and_lines() {
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

void main_window_tests::
    feedback_state_tracks_log_details_and_motion_throttle() {
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

void main_window_tests::
    stream_catalog_state_tracks_settings_and_local_sources() {
    stream_catalog_state catalog_state;

    const stream_settings fallback_settings
        = catalog_state.settings_for(QStringLiteral(" cam-1 "));
    QCOMPARE(fallback_settings.stream_name, QStringLiteral("cam-1"));
    QVERIFY(fallback_settings.labels_enabled);
    QCOMPARE(fallback_settings.algorithm_id, QStringLiteral("motion_baseline"));
    QCOMPARE(fallback_settings.algorithm_preset, QStringLiteral("balanced"));
    QVERIFY(!fallback_settings.manual_processing_policy_enabled);
    QCOMPARE(
        fallback_settings.manual_display_fps, default_manual_display_fps()
    );
    QCOMPARE(fallback_settings.manual_core_fps, default_manual_core_fps());
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
            .manual_core_fps = 11,
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
    QCOMPARE(saved_settings.manual_core_fps, 11);
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
        locals,
        QStringList({ QStringLiteral("video0"), QStringLiteral("video2") })
    );
}

void main_window_tests::stream_catalog_tracks_seed_add_and_local_sources() {
    settings_panel panel;
    stream_board board;
    stream_widget_bridge widget_bridge(&board, &panel);
    stream_catalog_state catalog_state;
    yodau::core::stream_manager stream_mgr;
    stream_catalog_workflow workflow(
        &stream_mgr, &panel, catalog_state, widget_bridge
    );

    stream_mgr.add_stream("/tmp/cam-1.mp4", "cam-1", "file", true);
    workflow.seed_from_core();

    auto* streams_list = panel.findChild<QTreeWidget*>(
        QStringLiteral("settings_streams_list")
    );
    QVERIFY(streams_list != nullptr);
    auto stream_row_for_name = [streams_list](const QString& name) -> int {
        for (int row = 0; row < streams_list->topLevelItemCount(); row += 1) {
            const auto* item = streams_list->topLevelItem(row);
            if (item != nullptr && item->text(1) == name) {
                return row;
            }
        }
        return -1;
    };

    const int initial_count = streams_list->topLevelItemCount();
    QVERIFY(initial_count >= 1);
    QVERIFY(stream_row_for_name(QStringLiteral("cam-1")) >= 0);
    QCOMPARE(
        catalog_state.settings_for(QStringLiteral("cam-1")).stream_name,
        QStringLiteral("cam-1")
    );

    workflow.seed_from_core();
    QCOMPARE(streams_list->topLevelItemCount(), initial_count);

    const auto add_result = workflow.add_stream(
        QStringLiteral("/tmp/cam-2.mp4"), QStringLiteral("cam-2"),
        QStringLiteral("file"), true
    );
    QCOMPARE(add_result.entries.size(), 1);
    QCOMPARE(
        add_result.entries.front().message, QStringLiteral("stream added")
    );
    QCOMPARE(add_result.entries.front().stream_name, QStringLiteral("cam-2"));
    QCOMPARE(add_result.added_stream, QStringLiteral("cam-2"));
    QCOMPARE(
        add_result.entries.front().detail, QStringLiteral("file:/tmp/cam-2.mp4")
    );
    QVERIFY(add_result.refresh_fps);
    QVERIFY(add_result.update_monitor_inventory);
    QCOMPARE(add_result.monitor_marker, QStringLiteral("stream_added"));
    QCOMPARE(streams_list->topLevelItemCount(), initial_count + 1);
    QVERIFY(stream_row_for_name(QStringLiteral("cam-2")) >= 0);
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
        invalid_url_result.entries.front().message,
        QStringLiteral("invalid url")
    );
    QVERIFY(!invalid_url_result.refresh_fps);
    QCOMPARE(streams_list->topLevelItemCount(), initial_count + 1);

    const auto local_result = workflow.detect_local_sources();
    QCOMPARE(local_result.entries.size(), 1);
    QCOMPARE(
        local_result.entries.front().message,
        QStringLiteral("local source inventory refreshed")
    );
    QVERIFY(
        local_result.entries.front().detail.contains(QStringLiteral("core="))
    );
    QVERIFY(
        local_result.entries.front().detail.contains(QStringLiteral("qt="))
    );
    QVERIFY(!local_result.refresh_fps);
    QVERIFY(local_result.update_monitor_inventory);
    QCOMPARE(
        local_result.monitor_marker, QStringLiteral("local_sources_refreshed")
    );

    auto* local_sources_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_add_local_sources_combo")
    );
    QVERIFY(local_sources_combo != nullptr);
}

void main_window_tests::controller_adds_and_focuses_file_and_local_sources() {
    yodau::core::stream_manager manager;
    settings_panel settings;
    stream_board board;
    stream_controller controller(&manager, &settings, &board);

    controller.handle_add_file(
        QStringLiteral("/tmp/mobile-file.mp4"), QStringLiteral("mobile-file"),
        true
    );
    controller.focus_stream(QStringLiteral("mobile-file"));
    QVERIFY(board.active_cell() != nullptr);
    QCOMPARE(board.active_cell()->get_name(), QStringLiteral("mobile-file"));

    controller.handle_add_local(
        QStringLiteral("opaque-camera-id"), QStringLiteral("mobile-camera")
    );
    controller.focus_stream(QStringLiteral("mobile-camera"));
    QVERIFY(board.active_cell() != nullptr);
    QCOMPARE(board.active_cell()->get_name(), QStringLiteral("mobile-camera"));
    QVERIFY(board.grid_mode()->has_stream(QStringLiteral("mobile-file")));

    auto* streams_list = settings.findChild<QTreeWidget*>(
        QStringLiteral("settings_streams_list")
    );
    QVERIFY(streams_list != nullptr);
    int checked_rows = 0;
    for (int row = 0; row < streams_list->topLevelItemCount(); row += 1) {
        if (streams_list->topLevelItem(row)->checkState(0) == Qt::Checked) {
            checked_rows += 1;
        }
    }
    QCOMPARE(checked_rows, 2);
}

void main_window_tests::stream_route_tracks_active_stream_and_add_validation() {
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
    stream_bridge_applies_active_state_and_template_preview() {
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
    QCOMPARE(panel.current_active_template_settings().template_name, QString());
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

void main_window_tests::stream_widget_bridge_syncs_visible_log_mode() {
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

void main_window_tests::
    stream_inventory_panel_tracks_entries_and_visibility_signal() {
    stream_inventory_panel panel;
    QSignalSpy show_spy(&panel, &stream_inventory_panel::show_stream_changed);

    auto* summary_label = panel.findChild<QLabel*>(
        QStringLiteral("settings_streams_summary_label")
    );
    QVERIFY(summary_label != nullptr);
    QVERIFY(
        summary_label->text().contains(QStringLiteral("No configured streams"))
    );

    panel.add_stream_entry(
        QStringLiteral("cam-1"), QStringLiteral("file:/tmp/demo.mp4")
    );
    panel.add_stream_entry(
        QStringLiteral("cam-2"), QStringLiteral("url:rtsp://cam-2"), true
    );

    auto* streams_list = panel.findChild<QTreeWidget*>(
        QStringLiteral("settings_streams_list")
    );
    QVERIFY(streams_list != nullptr);
    QCOMPARE(streams_list->topLevelItemCount(), 2);
    QCOMPARE(streams_list->topLevelItem(0)->text(1), QStringLiteral("cam-1"));
    QCOMPARE(streams_list->topLevelItem(1)->checkState(0), Qt::Checked);
    QVERIFY(summary_label->text().contains(QStringLiteral("2 configured")));
    QVERIFY(summary_label->text().contains(QStringLiteral("1 shown in grid")));

    streams_list->topLevelItem(0)->setCheckState(0, Qt::Checked);
    QCOMPARE(show_spy.size(), 1);
    QCOMPARE(show_spy.at(0).at(0).toString(), QStringLiteral("cam-1"));
    QCOMPARE(show_spy.at(0).at(1).toBool(), true);
    QVERIFY(summary_label->text().contains(QStringLiteral("2 shown in grid")));

    panel.remove_stream_entry(QStringLiteral("cam-2"));
    QCOMPARE(streams_list->topLevelItemCount(), 1);
    QVERIFY(summary_label->text().contains(QStringLiteral("1 configured")));

    panel.clear_stream_entries();
    QCOMPARE(streams_list->topLevelItemCount(), 0);
    QVERIFY(
        summary_label->text().contains(QStringLiteral("No configured streams"))
    );
}

void main_window_tests::
    stream_source_panel_tracks_modes_validation_and_requests() {
    qRegisterMetaType<app_log_entry>("app_log_entry");

    stream_source_panel panel;
    panel.set_existing_names({ QStringLiteral("dup") });
    panel.set_local_sources(
        QList<local_source_descriptor> {
            local_source_descriptor {
                .id = QStringLiteral("opaque-camera-id"),
                .display_name = QStringLiteral("Rear camera"),
            },
        }
    );

    auto* name_edit
        = panel.findChild<QLineEdit*>(QStringLiteral("settings_add_name_edit"));
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
    auto* url_edit
        = panel.findChild<QLineEdit*>(QStringLiteral("settings_add_url_edit"));
    auto* add_button
        = panel.findChild<QPushButton*>(QStringLiteral("settings_add_button"));
    auto* summary_label = panel.findChild<QLabel*>(
        QStringLiteral("settings_add_summary_label")
    );

    QVERIFY(name_edit != nullptr);
    QVERIFY(local_radio != nullptr);
    QVERIFY(url_radio != nullptr);
    QVERIFY(local_sources_combo != nullptr);
    QVERIFY(refresh_local_button != nullptr);
    QVERIFY(url_edit != nullptr);
    QVERIFY(add_button != nullptr);
    QVERIFY(summary_label != nullptr);

    QSignalSpy add_local_spy(&panel, &stream_source_panel::add_local_stream);
    QSignalSpy add_url_spy(&panel, &stream_source_panel::add_url_stream);
    QSignalSpy refresh_spy(
        &panel, &stream_source_panel::detect_local_sources_requested
    );
    QSignalSpy log_spy(&panel, &stream_source_panel::log_requested);

    local_radio->click();
    name_edit->setText(QStringLiteral("dup"));
    QVERIFY(!add_button->isEnabled());
    QVERIFY(summary_label->text().contains(
        QStringLiteral("local source Rear camera")
    ));

    name_edit->setText(QStringLiteral(" cam-1 "));
    QCOMPARE(local_sources_combo->currentText(), QStringLiteral("Rear camera"));
    QCOMPARE(
        local_sources_combo->currentData().toString(),
        QStringLiteral("opaque-camera-id")
    );
    QVERIFY(add_button->isEnabled());
    QVERIFY(summary_label->text().contains(QStringLiteral("ready")));
    QVERIFY(summary_label->text().contains(QStringLiteral("cam-1")));

    add_button->click();
    QCOMPARE(add_local_spy.size(), 1);
    QCOMPARE(
        add_local_spy.at(0).at(0).toString(), QStringLiteral("opaque-camera-id")
    );
    QCOMPARE(add_local_spy.at(0).at(1).toString(), QStringLiteral("cam-1"));
    QVERIFY(!log_spy.empty());
    QCOMPARE(
        qvariant_cast<app_log_entry>(log_spy.at(0).at(0)).message,
        QStringLiteral("requested local stream add")
    );

    refresh_local_button->click();
    QCOMPARE(refresh_spy.size(), 1);
    QVERIFY(log_spy.size() >= 2);
    QCOMPARE(
        qvariant_cast<app_log_entry>(log_spy.at(1).at(0)).message,
        QStringLiteral("local source detection requested")
    );

    url_radio->click();
    QVERIFY(!add_button->isEnabled());
    QVERIFY(summary_label->text().contains(QStringLiteral("URL mode waits")));

    url_edit->setText(QStringLiteral("rtsp://example.test/live"));
    QVERIFY(add_button->isEnabled());
    QVERIFY(summary_label->text().contains(QStringLiteral("url")));
    QVERIFY(summary_label->text().contains(
        QStringLiteral("rtsp://example.test/live")
    ));
    add_button->click();
    QCOMPARE(add_url_spy.size(), 1);
    QCOMPARE(
        add_url_spy.at(0).at(0).toString(),
        QStringLiteral("rtsp://example.test/live")
    );
    QCOMPARE(add_url_spy.at(0).at(1).toString(), QStringLiteral("cam-1"));
    QVERIFY(log_spy.size() >= 3);
    QCOMPARE(
        qvariant_cast<app_log_entry>(log_spy.at(2).at(0)).message,
        QStringLiteral("requested url stream add")
    );

    panel.clear_inputs();
    QCOMPARE(name_edit->text(), QString());
    QCOMPARE(url_edit->text(), QString());
    QVERIFY(!add_button->isEnabled());
    QVERIFY(summary_label->text().contains(QStringLiteral("URL mode waits")));
}

void main_window_tests::
    active_stream_panel_tracks_selection_modes_and_settings() {
    active_stream_panel panel(
        active_stream_panel::panel_mode::stream_settings,
        QStringLiteral("settings_active")
    );
    panel.set_active_candidates(
        { QStringLiteral("cam-1"), QStringLiteral("cam-2") }
    );

    QSignalSpy stream_selected_spy(
        &panel, &active_stream_panel::stream_selected
    );
    QSignalSpy stream_settings_spy(
        &panel, &active_stream_panel::stream_settings_changed
    );

    auto* active_stream_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_stream_combo")
    );
    auto* active_labels_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_active_labels_checkbox")
    );
    auto* standard_labels_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_active_standard_labels_checkbox")
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
    auto* algorithm_advanced_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_active_algorithm_advanced_checkbox")
    );
    auto* movement_display_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_movement_display_combo")
    );
    auto* manual_processing_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_active_manual_processing_checkbox")
    );
    auto* processing_advanced_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_active_processing_advanced_checkbox")
    );
    auto* manual_display_fps_spin = panel.findChild<QSpinBox*>(
        QStringLiteral("settings_active_display_fps_spin")
    );
    auto* manual_core_fps_spin = panel.findChild<QSpinBox*>(
        QStringLiteral("settings_active_core_fps_spin")
    );
    auto* manual_processing_pixels_spin = panel.findChild<QSpinBox*>(
        QStringLiteral("settings_active_processing_pixels_spin")
    );
    auto* template_radio = panel.findChild<QRadioButton*>(
        QStringLiteral("settings_active_mode_template_radio")
    );

    QVERIFY(active_stream_combo != nullptr);
    QVERIFY(active_labels_checkbox != nullptr);
    QVERIFY(standard_labels_checkbox != nullptr);
    QVERIFY(operator_profile_combo != nullptr);
    QVERIFY(operator_profile_summary != nullptr);
    QVERIFY(algorithm_combo != nullptr);
    QVERIFY(preset_combo != nullptr);
    QVERIFY(algorithm_advanced_checkbox != nullptr);
    QVERIFY(movement_display_combo != nullptr);
    QVERIFY(processing_advanced_checkbox != nullptr);
    QVERIFY(manual_processing_checkbox != nullptr);
    QVERIFY(manual_display_fps_spin != nullptr);
    QVERIFY(manual_core_fps_spin != nullptr);
    QVERIFY(manual_processing_pixels_spin != nullptr);
    QVERIFY(template_radio != nullptr);
    QVERIFY(!template_radio->isVisible());

    QVERIFY(!panel.has_active_stream());
    QVERIFY(panel.drawing_new_mode());

    active_stream_combo->setCurrentText(QStringLiteral("cam-1"));
    QCOMPARE(stream_selected_spy.size(), 1);
    QCOMPARE(
        stream_selected_spy.at(0).at(0).toString(), QStringLiteral("cam-1")
    );
    QCOMPARE(stream_settings_spy.size(), 0);
    {
        const stream_settings settings_value = panel.current_stream_settings();
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(settings_value.labels_enabled, true);
        QCOMPARE(settings_value.standard_labels_enabled, true);
        QCOMPARE(
            settings_value.algorithm_id, QStringLiteral("motion_baseline")
        );
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("balanced"));
        QVERIFY(!settings_value.manual_processing_policy_enabled);
        QCOMPARE(
            settings_value.manual_display_fps, default_manual_display_fps()
        );
        QCOMPARE(settings_value.manual_core_fps, default_manual_core_fps());
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
    QVERIFY(!algorithm_advanced_checkbox->isChecked());
    QVERIFY(preset_combo->isHidden());
    QVERIFY(!movement_display_combo->isHidden());
    QVERIFY(!processing_advanced_checkbox->isChecked());
    QVERIFY(manual_processing_checkbox->isHidden());
    QVERIFY(manual_display_fps_spin->isHidden());
    QVERIFY(manual_core_fps_spin->isHidden());
    QVERIFY(manual_processing_pixels_spin->isHidden());

    const int contour_mask_index
        = algorithm_combo->findData(QStringLiteral("contour_mask"));
    QVERIFY(contour_mask_index >= 0);
    algorithm_combo->setCurrentIndex(contour_mask_index);
    QCOMPARE(stream_settings_spy.size(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(settings_value.algorithm_id, QStringLiteral("contour_mask"));
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
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
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

    algorithm_advanced_checkbox->setChecked(true);
    QVERIFY(!preset_combo->isHidden());
    QVERIFY(!movement_display_combo->isHidden());

    const int movement_off_index
        = movement_display_combo->findData(QStringLiteral("off"));
    QVERIFY(movement_off_index >= 0);
    movement_display_combo->setCurrentIndex(movement_off_index);
    QCOMPARE(stream_settings_spy.size(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
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
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QVERIFY(!settings_value.labels_enabled);
        QCOMPARE(settings_value.algorithm_id, QStringLiteral("contour_mask"));
    }

    QVERIFY(active_labels_checkbox->toolTip().contains(
        QStringLiteral("saved line names")
    ));
    QVERIFY(standard_labels_checkbox->toolTip().contains(
        QStringLiteral("algorithm badges")
    ));

    standard_labels_checkbox->setChecked(false);
    QCOMPARE(stream_settings_spy.size(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QVERIFY(!settings_value.standard_labels_enabled);
    }

    processing_advanced_checkbox->setChecked(true);
    QVERIFY(!manual_processing_checkbox->isHidden());
    QVERIFY(!manual_display_fps_spin->isHidden());
    QVERIFY(!manual_core_fps_spin->isHidden());
    QVERIFY(!manual_processing_pixels_spin->isHidden());

    manual_processing_checkbox->setChecked(true);
    QCOMPARE(stream_settings_spy.size(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QVERIFY(settings_value.manual_processing_policy_enabled);
        QCOMPARE(
            settings_value.manual_display_fps, default_manual_display_fps()
        );
    }

    manual_display_fps_spin->setValue(18);
    manual_core_fps_spin->setValue(9);
    manual_processing_pixels_spin->setValue(320 * 180);
    QCOMPARE(stream_settings_spy.size(), 3);
    {
        const auto args = stream_settings_spy.takeLast();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QVERIFY(settings_value.manual_processing_policy_enabled);
        QCOMPARE(settings_value.manual_display_fps, 18);
        QCOMPARE(settings_value.manual_core_fps, 9);
        QCOMPARE(settings_value.manual_processing_pixels, 320 * 180);
    }
    stream_settings_spy.clear();

    panel.set_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-2"),
            .labels_enabled = true,
            .algorithm_id = QStringLiteral("spot grid"),
            .algorithm_preset = QStringLiteral("dense"),
            .movement_display_mode = QStringLiteral("off"),
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

void main_window_tests::log_toolbar_panel_filters_formats_and_emits_actions() {
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

void main_window_tests::active_editor_panel_tracks_active_tools_and_log() {
    active_editor_panel panel;
    panel.set_active_candidates({ QStringLiteral("cam-1") });
    panel.set_template_candidates({ QStringLiteral("north") });
    panel.set_active_current(QStringLiteral("cam-1"));
    panel.set_active_lines(
        {
            stream_cell::line_instance {
                .template_name = QStringLiteral("north"),
                .color = QColor(Qt::green),
                .color_mode_id = QStringLiteral("manual"),
                .enabled = true,
                .closed = false,
                .width_text = QStringLiteral("string_heavy"),
                .length_text = QStringLiteral("long"),
                .response_text = QStringLiteral("resonant"),
                .pts_pct = {
                    QPointF(10.0, 10.0),
                    QPointF(90.0, 15.0),
                },
            },
            stream_cell::line_instance {
                .template_name = QStringLiteral("south"),
                .color = QColor(Qt::yellow),
                .color_mode_id = QStringLiteral("negative_auto"),
                .enabled = false,
                .closed = true,
                .width_text = QStringLiteral("thin"),
                .length_text = QStringLiteral("short"),
                .response_text = QStringLiteral("dry"),
                .pts_pct = {
                    QPointF(10.0, 80.0),
                    QPointF(90.0, 85.0),
                    QPointF(50.0, 60.0),
                },
            },
        }
    );

    QSignalSpy active_stream_spy(
        &panel, &active_editor_panel::active_stream_selected
    );
    QSignalSpy line_enabled_spy(
        &panel, &active_editor_panel::line_enabled_changed
    );
    QSignalSpy line_detach_spy(
        &panel, &active_editor_panel::line_detach_requested
    );
    QObject::connect(
        &panel, &active_editor_panel::line_enabled_changed, &panel,
        [&panel](const QString& line_name, const bool enabled) {
            auto lines = std::vector<stream_cell::line_instance> {
                stream_cell::line_instance {
                    .template_name = QStringLiteral("north"),
                    .color = QColor(Qt::green),
                    .color_mode_id = QStringLiteral("manual"),
                    .enabled = line_name == QStringLiteral("north") ? enabled : true,
                    .closed = false,
                    .width_text = QStringLiteral("string_heavy"),
                    .length_text = QStringLiteral("long"),
                    .response_text = QStringLiteral("resonant"),
                    .pts_pct = {
                        QPointF(10.0, 10.0),
                        QPointF(90.0, 15.0),
                    },
                },
                stream_cell::line_instance {
                    .template_name = QStringLiteral("south"),
                    .color = QColor(Qt::yellow),
                    .color_mode_id = QStringLiteral("negative_auto"),
                    .enabled = line_name == QStringLiteral("south") ? enabled : false,
                    .closed = true,
                    .width_text = QStringLiteral("thin"),
                    .length_text = QStringLiteral("short"),
                    .response_text = QStringLiteral("dry"),
                    .pts_pct = {
                        QPointF(10.0, 80.0),
                        QPointF(90.0, 85.0),
                        QPointF(50.0, 60.0),
                    },
                },
            };
            panel.set_active_lines(lines);
        }
    );
    QObject::connect(
        &panel, &active_editor_panel::line_detach_requested, &panel,
        [&panel](const QString& line_name) {
            auto lines = std::vector<stream_cell::line_instance> {
                stream_cell::line_instance {
                    .template_name = QStringLiteral("north"),
                    .color = QColor(Qt::green),
                    .color_mode_id = QStringLiteral("manual"),
                    .enabled = true,
                    .closed = false,
                    .width_text = QStringLiteral("string_heavy"),
                    .length_text = QStringLiteral("long"),
                    .response_text = QStringLiteral("resonant"),
                    .pts_pct = {
                        QPointF(10.0, 10.0),
                        QPointF(90.0, 15.0),
                    },
                },
                stream_cell::line_instance {
                    .template_name = QStringLiteral("south"),
                    .color = QColor(Qt::yellow),
                    .color_mode_id = QStringLiteral("negative_auto"),
                    .enabled = false,
                    .closed = true,
                    .width_text = QStringLiteral("thin"),
                    .length_text = QStringLiteral("short"),
                    .response_text = QStringLiteral("dry"),
                    .pts_pct = {
                        QPointF(10.0, 80.0),
                        QPointF(90.0, 85.0),
                        QPointF(50.0, 60.0),
                    },
                },
            };
            lines.erase(
                std::remove_if(
                    lines.begin(), lines.end(),
                    [&line_name](const stream_cell::line_instance& line_value) {
                        return line_value.template_name == line_name;
                    }
                ),
                lines.end()
            );
            panel.set_active_lines(lines);
        }
    );

    auto* line_panel = panel.findChild<QGroupBox*>(
        QStringLiteral("settings_active_line_profile_panel")
    );
    auto* template_panel = panel.findChild<QGroupBox*>(
        QStringLiteral("settings_active_template_apply_panel")
    );
    auto* template_radio = panel.findChild<QRadioButton*>(
        QStringLiteral("settings_active_mode_template_radio")
    );
    auto* status_summary = panel.findChild<QLabel*>(
        QStringLiteral("settings_active_status_summary_label")
    );
    auto* line_summary = panel.findChild<QLabel*>(
        QStringLiteral("settings_active_lines_summary_label")
    );
    auto* editor_tabs = panel.findChild<QTabWidget*>(
        QStringLiteral("settings_active_editor_tabs")
    );
    auto* line_list = panel.findChild<QTreeWidget*>(
        QStringLiteral("settings_active_lines_list")
    );
    auto* detach_line_button = panel.findChild<QPushButton*>(
        QStringLiteral("settings_active_detach_line_button")
    );
    auto* active_stream_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_stream_combo")
    );

    QVERIFY(line_panel != nullptr);
    QVERIFY(template_panel != nullptr);
    QVERIFY(template_radio != nullptr);
    QVERIFY(status_summary != nullptr);
    QVERIFY(line_summary != nullptr);
    QVERIFY(editor_tabs != nullptr);
    QVERIFY(line_list != nullptr);
    QVERIFY(detach_line_button != nullptr);
    QVERIFY(active_stream_combo != nullptr);
    QVERIFY(
        editor_tabs->widget(0)->findChild<QRadioButton*>(
            QStringLiteral("settings_active_mode_template_radio")
        )
        != nullptr
    );

    QVERIFY(!line_panel->isHidden());
    QVERIFY(line_panel->isEnabled());
    QVERIFY(template_panel->isHidden());
    QVERIFY(status_summary->text().contains(QStringLiteral("cam-1")));
    QVERIFY(status_summary->text().contains(QStringLiteral("draw new")));
    QVERIFY(status_summary->text().contains(QStringLiteral("1 enabled of 2")));
    QVERIFY(line_summary->text().contains(QStringLiteral("2 saved lines")));
    QVERIFY(!detach_line_button->isEnabled());
    QCOMPARE(line_list->topLevelItemCount(), 2);
    QCOMPARE(line_list->topLevelItem(0)->checkState(0), Qt::Checked);
    QCOMPARE(line_list->topLevelItem(1)->checkState(0), Qt::Unchecked);

    template_radio->click();
    QVERIFY(line_panel->isHidden());
    QVERIFY(!template_panel->isHidden());
    QVERIFY(template_panel->isEnabled());
    QVERIFY(status_summary->text().contains(QStringLiteral("use template")));

    line_list->topLevelItem(1)->setCheckState(0, Qt::Checked);
    QCoreApplication::processEvents();
    QCOMPARE(line_enabled_spy.size(), 1);
    QCOMPARE(line_enabled_spy.at(0).at(0).toString(), QStringLiteral("south"));
    QCOMPARE(line_enabled_spy.at(0).at(1).toBool(), true);

    line_list->setCurrentItem(line_list->topLevelItem(1));
    QVERIFY(detach_line_button->isEnabled());
    detach_line_button->click();
    QCoreApplication::processEvents();
    QCOMPARE(line_detach_spy.size(), 1);
    QCOMPARE(line_detach_spy.at(0).at(0).toString(), QStringLiteral("south"));
    QCOMPARE(line_list->topLevelItemCount(), 1);
    QVERIFY(line_summary->text().contains(QStringLiteral("1 saved lines")));

    active_stream_combo->setCurrentIndex(0);
    active_stream_combo->setCurrentText(QStringLiteral("cam-1"));
    QVERIFY(!active_stream_spy.empty());
    QCOMPARE(
        active_stream_spy.takeLast().at(0).toString(), QStringLiteral("cam-1")
    );
}

void main_window_tests::active_editor_exposes_line_edit_tab_and_signals() {
    qRegisterMetaType<line_edit_request>("line_edit_request");

    active_editor_panel panel;
    panel.set_active_candidates({ QStringLiteral("cam-1") });
    panel.set_active_current(QStringLiteral("cam-1"));
    panel.set_active_lines(
        {
            stream_cell::line_instance {
                .template_name = QStringLiteral("north"),
                .color = QColor(Qt::green),
                .color_mode_id = QStringLiteral("manual"),
                .enabled = true,
                .closed = false,
                .width_text = QStringLiteral("string_heavy"),
                .length_text = QStringLiteral("long"),
                .response_text = QStringLiteral("resonant"),
                .pts_pct = {
                    QPointF(10.0, 10.0),
                    QPointF(50.0, 20.0),
                    QPointF(90.0, 25.0),
                },
            },
            stream_cell::line_instance {
                .template_name = QStringLiteral("south"),
                .color = QColor(Qt::yellow),
                .color_mode_id = QStringLiteral("negative_auto"),
                .enabled = false,
                .closed = true,
                .width_text = QStringLiteral("thin"),
                .length_text = QStringLiteral("short"),
                .response_text = QStringLiteral("dry"),
                .pts_pct = {
                    QPointF(20.0, 80.0),
                    QPointF(80.0, 75.0),
                },
            },
        }
    );

    QSignalSpy preview_spy(
        &panel, &active_editor_panel::line_edit_preview_changed
    );
    QSignalSpy clear_spy(
        &panel, &active_editor_panel::line_edit_preview_cleared
    );
    QSignalSpy save_spy(&panel, &active_editor_panel::line_edit_save_requested);

    auto* editor_tabs = panel.findChild<QTabWidget*>(
        QStringLiteral("settings_active_editor_tabs")
    );
    auto* line_edit_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_edit_line_combo")
    );
    auto* line_edit_table = panel.findChild<QTableWidget*>(
        QStringLiteral("settings_active_edit_points_table")
    );
    auto* line_edit_name = panel.findChild<QLineEdit*>(
        QStringLiteral("active_edit_new_name_edit")
    );
    auto* line_edit_save_button = panel.findChild<QPushButton*>(
        QStringLiteral("settings_active_edit_save_button")
    );
    auto* line_edit_summary = panel.findChild<QLabel*>(
        QStringLiteral("settings_active_edit_summary_label")
    );

    QVERIFY(editor_tabs != nullptr);
    QVERIFY(line_edit_combo != nullptr);
    QVERIFY(line_edit_table != nullptr);
    QVERIFY(line_edit_name != nullptr);
    QVERIFY(line_edit_save_button != nullptr);
    QVERIFY(line_edit_summary != nullptr);
    QCOMPARE(editor_tabs->count(), 3);
    QCOMPARE(editor_tabs->tabText(2), QStringLiteral("edit"));
    QVERIFY(line_edit_summary->text().contains(
        QStringLiteral("Select an enabled line")
    ));
    QCOMPARE(line_edit_combo->count(), 2);
    QCOMPARE(line_edit_combo->itemText(0), QStringLiteral("none"));
    QCOMPARE(line_edit_combo->itemText(1), QStringLiteral("north"));

    editor_tabs->setCurrentIndex(2);
    QCOMPARE(clear_spy.size(), 1);

    line_edit_combo->setCurrentIndex(1);
    QCOMPARE(preview_spy.size(), 1);
    {
        const auto args = preview_spy.takeFirst();
        const auto request = qvariant_cast<line_edit_request>(args.at(0));
        QCOMPARE(request.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(request.source_line_name, QStringLiteral("north"));
        QCOMPARE(request.profile.name, QStringLiteral("north_edit"));
        QCOMPARE(request.points_pct.size(), 3);
    }
    QCOMPARE(line_edit_table->rowCount(), 3);
    QCOMPARE(line_edit_table->item(0, 1)->text(), QStringLiteral("1"));
    QCOMPARE(line_edit_table->item(1, 2)->text(), QStringLiteral("50.00"));
    QCOMPARE(line_edit_table->item(2, 3)->text(), QStringLiteral("25.00"));
    QCOMPARE(line_edit_name->text(), QStringLiteral("north_edit"));
    QVERIFY(
        line_edit_summary->text().contains(QStringLiteral("keeping 3 of 3"))
    );

    line_edit_table->item(1, 0)->setCheckState(Qt::Unchecked);
    QCOMPARE(preview_spy.size(), 1);
    {
        const auto args = preview_spy.takeFirst();
        const auto request = qvariant_cast<line_edit_request>(args.at(0));
        QCOMPARE(request.points_pct.size(), 2);
    }
    QVERIFY(
        line_edit_summary->text().contains(QStringLiteral("keeping 2 of 3"))
    );

    line_edit_name->setText(QStringLiteral("north_variant"));
    QCOMPARE(preview_spy.size(), 1);
    {
        const auto args = preview_spy.takeFirst();
        const auto request = qvariant_cast<line_edit_request>(args.at(0));
        QCOMPARE(request.profile.name, QStringLiteral("north_variant"));
        QCOMPARE(request.points_pct.size(), 2);
    }
    QVERIFY(line_edit_save_button->isEnabled());

    line_edit_save_button->click();
    QCOMPARE(save_spy.size(), 1);
    {
        const auto args = save_spy.takeFirst();
        const auto request = qvariant_cast<line_edit_request>(args.at(0));
        QCOMPARE(request.source_line_name, QStringLiteral("north"));
        QCOMPARE(request.profile.name, QStringLiteral("north_variant"));
        QCOMPARE(request.points_pct.size(), 2);
    }
    QVERIFY(clear_spy.size() >= 2);
    QCOMPARE(line_edit_combo->currentIndex(), 0);
    QVERIFY(line_edit_name->text().isEmpty());
}

void main_window_tests::
    active_editor_applies_stream_driven_line_edit_updates() {
    qRegisterMetaType<line_edit_request>("line_edit_request");

    active_editor_panel panel;
    panel.set_active_candidates({ QStringLiteral("cam-1") });
    panel.set_active_current(QStringLiteral("cam-1"));
    panel.set_active_lines(
        {
            stream_cell::line_instance {
                .template_name = QStringLiteral("north"),
                .color = QColor(Qt::green),
                .color_mode_id = QStringLiteral("manual"),
                .enabled = true,
                .closed = false,
                .width_text = QStringLiteral("string_heavy"),
                .length_text = QStringLiteral("long"),
                .response_text = QStringLiteral("resonant"),
                .pts_pct = {
                    QPointF(10.0, 10.0),
                    QPointF(50.0, 20.0),
                    QPointF(90.0, 25.0),
                },
            },
        }
    );

    QSignalSpy preview_spy(
        &panel, &active_editor_panel::line_edit_preview_changed
    );

    auto* editor_tabs = panel.findChild<QTabWidget*>(
        QStringLiteral("settings_active_editor_tabs")
    );
    auto* line_edit_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_edit_line_combo")
    );
    auto* line_edit_table = panel.findChild<QTableWidget*>(
        QStringLiteral("settings_active_edit_points_table")
    );

    QVERIFY(editor_tabs != nullptr);
    QVERIFY(line_edit_combo != nullptr);
    QVERIFY(line_edit_table != nullptr);

    editor_tabs->setCurrentIndex(2);
    line_edit_combo->setCurrentIndex(1);
    QCOMPARE(preview_spy.size(), 1);
    preview_spy.clear();

    QVERIFY(panel.select_line_edit_point(1));
    QCOMPARE(line_edit_table->currentRow(), 1);

    QVERIFY(panel.move_line_edit_point(1, QPointF(55.0, 33.0)));
    QCOMPARE(preview_spy.size(), 1);
    {
        const auto args = preview_spy.takeFirst();
        const auto request = qvariant_cast<line_edit_request>(args.at(0));
        QCOMPARE(request.points_pct.size(), 3);
        QCOMPARE(request.points_pct.at(1), QPointF(55.0, 33.0));
    }
    QCOMPARE(line_edit_table->item(1, 2)->text(), QStringLiteral("55.00"));
    QCOMPARE(line_edit_table->item(1, 3)->text(), QStringLiteral("33.00"));

    QVERIFY(panel.translate_line_edit_shape(QPointF(5.0, -3.0)));
    QCOMPARE(preview_spy.size(), 1);
    {
        const auto args = preview_spy.takeFirst();
        const auto request = qvariant_cast<line_edit_request>(args.at(0));
        QCOMPARE(request.points_pct.at(0), QPointF(15.0, 7.0));
        QCOMPARE(request.points_pct.at(1), QPointF(60.0, 30.0));
        QCOMPARE(request.points_pct.at(2), QPointF(95.0, 22.0));
    }

    QVERIFY(panel.split_line_edit_point(1));
    QCOMPARE(preview_spy.size(), 1);
    {
        const auto args = preview_spy.takeFirst();
        const auto request = qvariant_cast<line_edit_request>(args.at(0));
        QCOMPARE(request.points_pct.size(), 4);
        QCOMPARE(request.points_pct.at(0), QPointF(15.0, 7.0));
        QCOMPARE(request.points_pct.at(1), QPointF(37.5, 18.5));
        QCOMPARE(request.points_pct.at(2), QPointF(77.5, 26.0));
        QCOMPARE(request.points_pct.at(3), QPointF(95.0, 22.0));
    }
    QCOMPARE(line_edit_table->rowCount(), 4);
    QCOMPARE(line_edit_table->item(1, 2)->text(), QStringLiteral("37.50"));
    QCOMPARE(line_edit_table->item(2, 2)->text(), QStringLiteral("77.50"));
}

void main_window_tests::active_editor_panel_rotates_line_edit_preview() {
    qRegisterMetaType<line_edit_request>("line_edit_request");

    active_editor_panel panel;
    panel.set_active_candidates({ QStringLiteral("cam-1") });
    panel.set_active_current(QStringLiteral("cam-1"));
    panel.set_active_lines(
        {
            stream_cell::line_instance {
                .template_name = QStringLiteral("north"),
                .color = QColor(Qt::green),
                .color_mode_id = QStringLiteral("manual"),
                .enabled = true,
                .closed = false,
                .width_text = QStringLiteral("string_heavy"),
                .length_text = QStringLiteral("long"),
                .response_text = QStringLiteral("resonant"),
                .pts_pct = {
                    QPointF(40.0, 50.0),
                    QPointF(50.0, 50.0),
                    QPointF(60.0, 50.0),
                },
            },
        }
    );

    QSignalSpy preview_spy(
        &panel, &active_editor_panel::line_edit_preview_changed
    );

    auto* editor_tabs = panel.findChild<QTabWidget*>(
        QStringLiteral("settings_active_editor_tabs")
    );
    auto* line_edit_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_edit_line_combo")
    );

    QVERIFY(editor_tabs != nullptr);
    QVERIFY(line_edit_combo != nullptr);

    editor_tabs->setCurrentIndex(2);
    line_edit_combo->setCurrentIndex(1);
    QCOMPARE(preview_spy.size(), 1);
    preview_spy.clear();

    QVERIFY(panel.rotate_line_edit_shape(180.0));
    QCOMPARE(preview_spy.size(), 1);
    {
        const auto args = preview_spy.takeFirst();
        const auto request = qvariant_cast<line_edit_request>(args.at(0));
        QCOMPARE(request.points_pct.size(), 3);
        QVERIFY(
            main_window_tests_support::point_close(
                request.points_pct.at(0), QPointF(60.0, 50.0)
            )
        );
        QVERIFY(
            main_window_tests_support::point_close(
                request.points_pct.at(1), QPointF(50.0, 50.0)
            )
        );
        QVERIFY(
            main_window_tests_support::point_close(
                request.points_pct.at(2), QPointF(40.0, 50.0)
            )
        );
    }

    QVERIFY(panel.select_line_edit_point(1));
    QVERIFY(panel.rotate_line_edit_shape(90.0, 1));
    QCOMPARE(preview_spy.size(), 1);
    {
        const auto args = preview_spy.takeFirst();
        const auto request = qvariant_cast<line_edit_request>(args.at(0));
        QCOMPARE(request.points_pct.size(), 3);
        QVERIFY(
            main_window_tests_support::point_close(
                request.points_pct.at(0), QPointF(50.0, 60.0)
            )
        );
        QVERIFY(
            main_window_tests_support::point_close(
                request.points_pct.at(1), QPointF(50.0, 50.0)
            )
        );
        QVERIFY(
            main_window_tests_support::point_close(
                request.points_pct.at(2), QPointF(50.0, 40.0)
            )
        );
    }
}

void main_window_tests::log_view_tracks_toolbar_refresh_and_fallback_append() {
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

void main_window_tests::app_settings_normalize_algorithm_and_line_width() {
    QCOMPARE(
        normalized_app_algorithm_id(QStringLiteral("contour mask")),
        QStringLiteral("contour_mask")
    );
    QCOMPARE(
        normalized_app_algorithm_id(QStringLiteral("spots")),
        QStringLiteral("spot_grid")
    );
    QCOMPARE(
        normalized_app_algorithm_id(QStringLiteral("auto")),
        QStringLiteral("hybrid_auto")
    );
    QCOMPARE(
        normalized_app_algorithm_id(QStringLiteral("tracking")),
        QStringLiteral("centroid_track")
    );
    QCOMPARE(
        normalized_app_algorithm_id(QString()),
        QStringLiteral("motion_baseline")
    );
    QCOMPARE(
        default_algorithm_preset_id(QStringLiteral("hybrid_auto")),
        QStringLiteral("adaptive")
    );
    QCOMPARE(
        normalized_algorithm_preset_id(
            QStringLiteral("hybrid_auto"), QStringLiteral("simple")
        ),
        QStringLiteral("load_guard")
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
    QCOMPARE(
        normalized_algorithm_preset_id(
            QStringLiteral("centroid_track"), QStringLiteral("debug")
        ),
        QStringLiteral("persistent")
    );
    QVERIFY(algorithm_summary_text(
                QStringLiteral("spot_grid"), QStringLiteral("dense"),
                QStringLiteral("auto")
    )
                .contains(QStringLiteral("point-style motion regions")));
    QVERIFY(algorithm_summary_text(
                QStringLiteral("hybrid_auto"), QStringLiteral("adaptive"),
                QStringLiteral("auto")
    )
                .contains(QStringLiteral("adapts between low-cost, tripwire")));
    QVERIFY(algorithm_summary_text(
                QStringLiteral("centroid_track"), QStringLiteral("persistent"),
                QStringLiteral("auto")
    )
                .contains(QStringLiteral("short tracks")));
    QCOMPARE(
        algorithm_badge_text(
            QStringLiteral("hybrid_auto"), QStringLiteral("load_guard"), false
        ),
        QStringLiteral("HA simple")
    );
    QCOMPARE(
        algorithm_badge_text(
            QStringLiteral("centroid_track"), QStringLiteral("persistent"), true
        ),
        QStringLiteral("CT debug-heavy")
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
    QVERIFY([] {
        stream_settings settings_value;
        settings_value.algorithm_id = QStringLiteral("contour_mask");
        settings_value.algorithm_preset = QStringLiteral("mask_heavy");
        settings_value.algorithm_overlay_enabled = true;
        return operator_profile_summary_text(settings_value);
    }()
                .contains(QStringLiteral("debug-heavy")));

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
        normalized_line_color_mode_id(QStringLiteral("negative")),
        QStringLiteral("negative_auto")
    );
    QCOMPARE(
        normalized_line_color_mode_id(QStringLiteral("random")),
        QStringLiteral("manual")
    );
    QVERIFY(random_manual_line_color().isValid());
    QVERIFY(auto_palette_line_color(1, 4).isValid());
    QVERIFY(softened_negative_line_color(QColor(QStringLiteral("#203040")))
                .isValid());
    QCOMPARE(
        normalized_line_width_text(QStringLiteral("6")), QStringLiteral("6.0")
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

void main_window_tests::format_app_log_entry_distinguishes_release_and_debug() {
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

void main_window_tests::settings_panel_filters_logs_by_area_and_mode() {
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

void main_window_tests::
    settings_filters_logs_by_severity_stream_and_subsystem() {
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

void main_window_tests::settings_panel_round_trips_structured_settings() {
    settings_panel panel;
    panel.set_existing_names(
        { QStringLiteral("cam-1"), QStringLiteral("cam-2") }
    );
    panel.set_template_candidates(
        { QStringLiteral("north"), QStringLiteral("south") }
    );

    panel.set_active_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-2"),
            .labels_enabled = false,
            .standard_labels_enabled = false,
            .algorithm_id = QStringLiteral("contour mask"),
            .algorithm_preset = QStringLiteral("mask_heavy"),
            .algorithm_overlay_enabled = true,
            .manual_processing_policy_enabled = true,
            .manual_display_fps = 19,
            .manual_core_fps = 8,
            .manual_processing_pixels = 640 * 360,
        }
    );

    const stream_settings active_stream
        = panel.current_active_stream_settings();
    QCOMPARE(active_stream.stream_name, QStringLiteral("cam-2"));
    QCOMPARE(active_stream.labels_enabled, false);
    QCOMPARE(active_stream.standard_labels_enabled, false);
    QCOMPARE(active_stream.algorithm_id, QStringLiteral("contour_mask"));
    QCOMPARE(active_stream.algorithm_preset, QStringLiteral("mask_heavy"));
    QCOMPARE(active_stream.algorithm_overlay_enabled, true);
    QVERIFY(active_stream.manual_processing_policy_enabled);
    QCOMPARE(active_stream.manual_display_fps, 19);
    QCOMPARE(active_stream.manual_core_fps, 8);
    QCOMPARE(active_stream.manual_processing_pixels, 640 * 360);

    panel.set_active_line_profile(
        line_profile {
            .name = QStringLiteral("north"),
            .color = QColor(Qt::green),
            .color_mode_id = QStringLiteral("manual"),
            .closed = true,
            .width_text = QStringLiteral("String Heavy"),
            .length_text = QStringLiteral("long"),
            .response_text = QStringLiteral("resonant"),
        }
    );

    const line_profile active_line = panel.current_active_line_profile();
    QCOMPARE(active_line.name, QStringLiteral("north"));
    QCOMPARE(active_line.color, QColor(Qt::green));
    QCOMPARE(active_line.color_mode_id, QStringLiteral("manual"));
    QCOMPARE(active_line.closed, true);
    QCOMPARE(active_line.width_text, QStringLiteral("string_heavy"));
    QCOMPARE(active_line.length_text, QStringLiteral("long"));
    QCOMPARE(active_line.response_text, QStringLiteral("resonant"));

    panel.set_active_template_settings(
        template_apply_settings {
            .template_name = QStringLiteral("south"),
            .color = QColor(Qt::yellow),
            .color_mode_id = QStringLiteral("negative"),
            .width_text = QStringLiteral("thin"),
            .length_text = QStringLiteral("short"),
            .response_text = QStringLiteral("dry"),
        }
    );

    const template_apply_settings active_template
        = panel.current_active_template_settings();
    QCOMPARE(active_template.template_name, QStringLiteral("south"));
    QCOMPARE(active_template.color, QColor(Qt::yellow));
    QCOMPARE(active_template.color_mode_id, QStringLiteral("negative_auto"));
    QCOMPARE(active_template.width_text, QStringLiteral("thin"));
    QCOMPARE(active_template.length_text, QStringLiteral("short"));
    QCOMPARE(active_template.response_text, QStringLiteral("dry"));
}

void main_window_tests::settings_panel_exposes_explicit_edit_panels() {
    settings_panel panel;
    panel.set_existing_names({ QStringLiteral("cam-1") });
    panel.set_template_candidates({ QStringLiteral("north") });
    stream_settings initial_settings;
    initial_settings.stream_name = QStringLiteral("cam-1");
    panel.set_active_stream_settings(initial_settings);

    auto* settings_tabs
        = panel.findChild<QTabWidget*>(QStringLiteral("settings_tabs"));
    QVERIFY(settings_tabs != nullptr);
    QCOMPARE(settings_tabs->count(), 2);
    QCOMPARE(settings_tabs->tabText(0), QStringLiteral("streams"));
    QCOMPARE(settings_tabs->tabText(1), QStringLiteral("stream settings"));
    QVERIFY(
        settings_tabs->widget(0)->findChild<QLineEdit*>(
            QStringLiteral("settings_add_name_edit")
        )
        != nullptr
    );
    QVERIFY(
        settings_tabs->widget(0)->findChild<QTreeWidget*>(
            QStringLiteral("settings_streams_list")
        )
        != nullptr
    );

    auto* line_editor = panel.take_active_editor_widget();
    QVERIFY(line_editor != nullptr);
    auto* line_editor_panel = qobject_cast<active_editor_panel*>(line_editor);
    QVERIFY(line_editor_panel != nullptr);
    line_editor_panel->set_active_candidates({ QStringLiteral("cam-1") });
    line_editor_panel->set_active_current(QStringLiteral("cam-1"));

    auto* line_panel = line_editor_panel->findChild<QGroupBox*>(
        QStringLiteral("settings_active_line_profile_panel")
    );
    auto* template_panel = line_editor_panel->findChild<QGroupBox*>(
        QStringLiteral("settings_active_template_apply_panel")
    );
    auto* line_summary = line_editor_panel->findChild<QLabel*>(
        QStringLiteral("settings_active_line_summary_label")
    );
    auto* line_color_mode_combo = line_editor_panel->findChild<QComboBox*>(
        QStringLiteral("active_line_color_mode_combo")
    );
    auto* line_color_button = line_editor_panel->findChild<QPushButton*>(
        QStringLiteral("settings_active_line_color_button")
    );
    auto* line_random_color_button = line_editor_panel->findChild<QPushButton*>(
        QStringLiteral("active_line_random_color_button")
    );
    auto* line_advanced_checkbox = line_editor_panel->findChild<QCheckBox*>(
        QStringLiteral("settings_active_line_advanced_checkbox")
    );
    auto* line_parameter_mode_combo = line_editor_panel->findChild<QComboBox*>(
        QStringLiteral("active_line_parameter_mode_combo")
    );
    auto* line_width_combo = line_editor_panel->findChild<QComboBox*>(
        QStringLiteral("settings_active_line_width_combo")
    );
    auto* line_length_combo = line_editor_panel->findChild<QComboBox*>(
        QStringLiteral("settings_active_line_length_combo")
    );
    auto* line_response_combo = line_editor_panel->findChild<QComboBox*>(
        QStringLiteral("settings_active_line_response_combo")
    );
    auto* template_summary = line_editor_panel->findChild<QLabel*>(
        QStringLiteral("settings_active_template_summary_label")
    );
    auto* template_color_mode_combo = line_editor_panel->findChild<QComboBox*>(
        QStringLiteral("active_template_color_mode_combo")
    );
    auto* template_color_button = line_editor_panel->findChild<QPushButton*>(
        QStringLiteral("settings_active_template_color_button")
    );
    auto* template_random_color_button
        = line_editor_panel->findChild<QPushButton*>(
            QStringLiteral("active_template_random_color_button")
        );
    auto* template_advanced_checkbox = line_editor_panel->findChild<QCheckBox*>(
        QStringLiteral("settings_active_template_advanced_checkbox")
    );
    auto* template_parameter_mode_combo
        = line_editor_panel->findChild<QComboBox*>(
            QStringLiteral("active_template_parameter_mode_combo")
        );
    auto* template_width_combo = line_editor_panel->findChild<QComboBox*>(
        QStringLiteral("settings_active_template_width_combo")
    );
    auto* template_length_combo = line_editor_panel->findChild<QComboBox*>(
        QStringLiteral("settings_active_template_length_combo")
    );
    auto* template_response_combo = line_editor_panel->findChild<QComboBox*>(
        QStringLiteral("settings_active_template_response_combo")
    );
    auto* active_status_summary = line_editor_panel->findChild<QLabel*>(
        QStringLiteral("settings_active_status_summary_label")
    );
    auto* line_list_summary = line_editor_panel->findChild<QLabel*>(
        QStringLiteral("settings_active_lines_summary_label")
    );
    auto* line_edit_summary = line_editor_panel->findChild<QLabel*>(
        QStringLiteral("settings_active_edit_summary_label")
    );
    auto* line_edit_combo = line_editor_panel->findChild<QComboBox*>(
        QStringLiteral("settings_active_edit_line_combo")
    );
    auto* line_edit_points_table = line_editor_panel->findChild<QTableWidget*>(
        QStringLiteral("settings_active_edit_points_table")
    );
    auto* line_edit_name_edit = line_editor_panel->findChild<QLineEdit*>(
        QStringLiteral("active_edit_new_name_edit")
    );
    auto* line_edit_save_button = line_editor_panel->findChild<QPushButton*>(
        QStringLiteral("settings_active_edit_save_button")
    );
    auto* editor_tabs = line_editor_panel->findChild<QTabWidget*>(
        QStringLiteral("settings_active_editor_tabs")
    );

    QVERIFY(line_panel != nullptr);
    QVERIFY(template_panel != nullptr);
    QVERIFY(line_summary != nullptr);
    QVERIFY(line_color_mode_combo != nullptr);
    QVERIFY(line_color_button != nullptr);
    QVERIFY(line_random_color_button != nullptr);
    QVERIFY(line_advanced_checkbox != nullptr);
    QVERIFY(line_parameter_mode_combo != nullptr);
    QVERIFY(line_width_combo != nullptr);
    QVERIFY(line_length_combo != nullptr);
    QVERIFY(line_response_combo != nullptr);
    QVERIFY(template_summary != nullptr);
    QVERIFY(template_color_mode_combo != nullptr);
    QVERIFY(template_color_button != nullptr);
    QVERIFY(template_random_color_button != nullptr);
    QVERIFY(template_advanced_checkbox != nullptr);
    QVERIFY(template_parameter_mode_combo != nullptr);
    QVERIFY(template_width_combo != nullptr);
    QVERIFY(template_length_combo != nullptr);
    QVERIFY(template_response_combo != nullptr);
    QVERIFY(active_status_summary != nullptr);
    QVERIFY(line_list_summary != nullptr);
    QVERIFY(line_edit_summary != nullptr);
    QVERIFY(line_edit_combo != nullptr);
    QVERIFY(line_edit_points_table != nullptr);
    QVERIFY(line_edit_name_edit != nullptr);
    QVERIFY(line_edit_save_button != nullptr);
    QVERIFY(editor_tabs != nullptr);
    QCOMPARE(editor_tabs->count(), 3);
    QCOMPARE(editor_tabs->tabText(2), QStringLiteral("edit"));
    QVERIFY(active_status_summary->text().contains(QStringLiteral("cam-1")));
    QVERIFY(line_list_summary->text().contains(
        QStringLiteral("No saved lines on this stream yet")
    ));
    QVERIFY(line_edit_summary->text().contains(
        QStringLiteral("Select an enabled line")
    ));
    QVERIFY(
        editor_tabs->widget(0)->findChild<QRadioButton*>(
            QStringLiteral("settings_active_mode_template_radio")
        )
        != nullptr
    );
    QCOMPARE(line_edit_combo->currentText(), QStringLiteral("none"));
    QCOMPARE(line_edit_points_table->rowCount(), 0);
    QVERIFY(line_edit_name_edit->text().isEmpty());
    QVERIFY(!line_edit_save_button->isEnabled());
    QVERIFY(!line_advanced_checkbox->isChecked());
    QVERIFY(line_parameter_mode_combo->isHidden());
    QVERIFY(line_width_combo->isHidden());
    QVERIFY(line_length_combo->isHidden());
    QVERIFY(line_response_combo->isHidden());
    QVERIFY(!template_advanced_checkbox->isChecked());
    QVERIFY(template_parameter_mode_combo->isHidden());
    QVERIFY(template_width_combo->isHidden());
    QVERIFY(template_length_combo->isHidden());
    QVERIFY(template_response_combo->isHidden());

    line_editor_panel->set_line_profile(
        line_profile {
            .name = QStringLiteral("north"),
            .color = QColor(Qt::cyan),
            .color_mode_id = QStringLiteral("manual"),
            .closed = false,
            .width_text = QStringLiteral("string_light"),
            .length_text = QStringLiteral("long"),
            .response_text = QStringLiteral("resonant"),
        }
    );
    line_editor_panel->set_template_settings(
        template_apply_settings {
            .template_name = QStringLiteral("north"),
            .color = QColor(Qt::magenta),
            .color_mode_id = QStringLiteral("negative"),
            .width_text = QStringLiteral("thick"),
            .length_text = QStringLiteral("short"),
            .response_text = QStringLiteral("dry"),
        }
    );

    QVERIFY(
        line_summary->text().contains(QStringLiteral("width=string_light"))
    );
    QVERIFY(line_summary->text().contains(QStringLiteral("length=long")));
    QVERIFY(line_summary->text().contains(QStringLiteral("response=resonant")));
    QVERIFY(line_summary->text().contains(QStringLiteral("manual color")));
    QVERIFY(!line_color_button->isHidden());
    QVERIFY(!line_random_color_button->isHidden());
    QVERIFY(line_random_color_button->isEnabled());

    QVERIFY(template_summary->text().contains(QStringLiteral("north")));
    QVERIFY(template_summary->text().contains(QStringLiteral("width=thick")));
    QVERIFY(template_summary->text().contains(QStringLiteral("length=short")));
    QVERIFY(template_summary->text().contains(QStringLiteral("response=dry")));
    QVERIFY(template_summary->text().contains(QStringLiteral("negative auto")));
    QVERIFY(template_color_button->isHidden());
    QVERIFY(template_random_color_button->isHidden());

    line_advanced_checkbox->setChecked(true);
    template_advanced_checkbox->setChecked(true);
    QVERIFY(!line_parameter_mode_combo->isHidden());
    QVERIFY(!line_width_combo->isHidden());
    QVERIFY(!line_length_combo->isHidden());
    QVERIFY(!line_response_combo->isHidden());
    QVERIFY(!template_parameter_mode_combo->isHidden());
    QVERIFY(!template_width_combo->isHidden());
    QVERIFY(!template_length_combo->isHidden());
    QVERIFY(!template_response_combo->isHidden());
}

void main_window_tests::settings_panel_emits_structured_stream_settings() {
    settings_panel panel;
    panel.set_existing_names({ QStringLiteral("cam-1") });
    stream_settings initial_settings;
    initial_settings.stream_name = QStringLiteral("cam-1");
    panel.set_active_stream_settings(initial_settings);

    QSignalSpy stream_settings_spy(
        &panel, &settings_panel::active_stream_settings_changed
    );

    auto* active_stream_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_stream_editor_stream_combo")
    );
    auto* active_algorithm_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_stream_editor_algorithm_combo")
    );
    auto* active_labels_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_stream_editor_labels_checkbox")
    );
    auto* standard_labels_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_stream_editor_standard_labels_checkbox")
    );
    auto* operator_profile_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_stream_editor_operator_profile_combo")
    );
    auto* algorithm_advanced_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_stream_editor_algorithm_advanced_checkbox")
    );
    auto* preset_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_stream_editor_algorithm_preset_combo")
    );
    auto* manual_processing_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_stream_editor_manual_processing_checkbox")
    );
    auto* processing_advanced_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_stream_editor_processing_advanced_checkbox")
    );
    auto* manual_display_fps_spin = panel.findChild<QSpinBox*>(
        QStringLiteral("settings_stream_editor_display_fps_spin")
    );
    auto* manual_core_fps_spin = panel.findChild<QSpinBox*>(
        QStringLiteral("settings_stream_editor_core_fps_spin")
    );
    auto* manual_processing_pixels_spin = panel.findChild<QSpinBox*>(
        QStringLiteral("settings_stream_editor_processing_pixels_spin")
    );

    QVERIFY(active_stream_combo != nullptr);
    QVERIFY(active_algorithm_combo != nullptr);
    QVERIFY(active_labels_checkbox != nullptr);
    QVERIFY(standard_labels_checkbox != nullptr);
    QVERIFY(operator_profile_combo != nullptr);
    QVERIFY(algorithm_advanced_checkbox != nullptr);
    QVERIFY(preset_combo != nullptr);
    QVERIFY(processing_advanced_checkbox != nullptr);
    QVERIFY(manual_processing_checkbox != nullptr);
    QVERIFY(manual_display_fps_spin != nullptr);
    QVERIFY(manual_core_fps_spin != nullptr);
    QVERIFY(manual_processing_pixels_spin != nullptr);
    QCOMPARE(active_stream_combo->currentText(), QStringLiteral("cam-1"));
    QVERIFY(!algorithm_advanced_checkbox->isChecked());
    QVERIFY(!active_algorithm_combo->isHidden());
    QVERIFY(preset_combo->isHidden());
    QVERIFY(manual_processing_checkbox->isHidden());

    const int contour_mask_index
        = active_algorithm_combo->findData(QStringLiteral("contour_mask"));
    QVERIFY(contour_mask_index >= 0);
    active_algorithm_combo->setCurrentIndex(contour_mask_index);

    QCOMPARE(stream_settings_spy.count(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(settings_value.algorithm_id, QStringLiteral("contour_mask"));
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("balanced"));
    }

    const int debug_heavy_index
        = operator_profile_combo->findData(QStringLiteral("debug_heavy"));
    QVERIFY(debug_heavy_index >= 0);
    operator_profile_combo->setCurrentIndex(debug_heavy_index);

    QCOMPARE(stream_settings_spy.count(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(settings_value.algorithm_id, QStringLiteral("contour_mask"));
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("mask_heavy"));
        QVERIFY(settings_value.algorithm_overlay_enabled);
    }

    algorithm_advanced_checkbox->setChecked(true);
    processing_advanced_checkbox->setChecked(true);
    QVERIFY(!manual_processing_checkbox->isHidden());
    manual_processing_checkbox->setChecked(true);
    manual_display_fps_spin->setValue(20);
    manual_core_fps_spin->setValue(10);
    manual_processing_pixels_spin->setValue(512 * 512);

    QVERIFY(stream_settings_spy.count() >= 3);
    {
        const auto args = stream_settings_spy.takeLast();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QVERIFY(settings_value.manual_processing_policy_enabled);
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("mask_heavy"));
        QVERIFY(settings_value.algorithm_overlay_enabled);
        QCOMPARE(settings_value.manual_display_fps, 20);
        QCOMPARE(settings_value.manual_core_fps, 10);
        QCOMPARE(settings_value.manual_processing_pixels, 512 * 512);
    }
    stream_settings_spy.clear();

    active_labels_checkbox->setChecked(false);

    QCOMPARE(stream_settings_spy.count(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(settings_value.labels_enabled, false);
        QCOMPARE(settings_value.algorithm_id, QStringLiteral("contour_mask"));
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("mask_heavy"));
        QVERIFY(settings_value.algorithm_overlay_enabled);
        QVERIFY(settings_value.manual_processing_policy_enabled);
        QCOMPARE(settings_value.manual_display_fps, 20);
    }
    stream_settings_spy.clear();

    standard_labels_checkbox->setChecked(false);

    QCOMPARE(stream_settings_spy.count(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QVERIFY(!settings_value.standard_labels_enabled);
        QCOMPARE(settings_value.algorithm_id, QStringLiteral("contour_mask"));
        QVERIFY(settings_value.manual_processing_policy_enabled);
    }
}

void main_window_tests::settings_panel_emits_log_mode_changes() {
    settings_panel panel;

    QSignalSpy log_mode_spy(&panel, &settings_panel::log_mode_changed);
    auto* log_mode_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_log_mode_combo")
    );

    QVERIFY(log_mode_combo != nullptr);
    QCOMPARE(panel.log_mode(), app_log_mode::release);

    log_mode_combo->setCurrentIndex(1);

    QCOMPARE(log_mode_spy.count(), 1);
    QCOMPARE(
        log_mode_spy.takeFirst().at(0).value<app_log_mode>(),
        app_log_mode::debug
    );
    QCOMPARE(panel.log_mode(), app_log_mode::debug);

    log_mode_combo->setCurrentIndex(0);

    QCOMPARE(log_mode_spy.count(), 1);
    QCOMPARE(
        log_mode_spy.takeFirst().at(0).value<app_log_mode>(),
        app_log_mode::release
    );
    QCOMPARE(panel.log_mode(), app_log_mode::release);
}

void main_window_tests::
    settings_panel_updates_algorithm_presets_by_selection() {
    settings_panel panel;
    panel.set_existing_names({ QStringLiteral("cam-1") });
    panel.set_active_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-1"),
            .labels_enabled = true,
            .algorithm_id = QStringLiteral("motion_baseline"),
            .algorithm_preset = QStringLiteral("balanced"),
            .movement_display_mode = QStringLiteral("off"),
            .algorithm_overlay_enabled = false,
        }
    );

    QSignalSpy stream_settings_spy(
        &panel, &settings_panel::active_stream_settings_changed
    );

    auto* algorithm_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_stream_editor_algorithm_combo")
    );
    auto* operator_profile_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_stream_editor_operator_profile_combo")
    );
    auto* algorithm_advanced_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_stream_editor_algorithm_advanced_checkbox")
    );
    auto* preset_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_stream_editor_algorithm_preset_combo")
    );
    auto* movement_display_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_stream_editor_movement_display_combo")
    );
    auto* summary_label = panel.findChild<QLabel*>(
        QStringLiteral("settings_stream_editor_algorithm_summary_label")
    );

    QVERIFY(algorithm_combo != nullptr);
    QVERIFY(operator_profile_combo != nullptr);
    QVERIFY(algorithm_advanced_checkbox != nullptr);
    QVERIFY(preset_combo != nullptr);
    QVERIFY(movement_display_combo != nullptr);
    QVERIFY(summary_label != nullptr);
    QVERIFY(!algorithm_advanced_checkbox->isChecked());
    QVERIFY(preset_combo->isHidden());
    QVERIFY(!movement_display_combo->isHidden());

    const int contour_mask_index
        = algorithm_combo->findData(QStringLiteral("contour_mask"));
    QVERIFY(contour_mask_index >= 0);
    algorithm_combo->setCurrentIndex(contour_mask_index);

    QCOMPARE(stream_settings_spy.count(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(settings_value.algorithm_id, QStringLiteral("contour_mask"));
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("balanced"));
        QCOMPARE(settings_value.algorithm_overlay_enabled, false);
    }

    QCOMPARE(
        preset_combo->currentData().toString(), QStringLiteral("balanced")
    );
    QCOMPARE(
        operator_profile_combo->currentData().toString(),
        QStringLiteral("custom")
    );
    QVERIFY(
        summary_label->text().contains(QStringLiteral("contours and masks"))
    );

    const int debug_heavy_index
        = operator_profile_combo->findData(QStringLiteral("debug_heavy"));
    QVERIFY(debug_heavy_index >= 0);
    operator_profile_combo->setCurrentIndex(debug_heavy_index);
    QCOMPARE(stream_settings_spy.count(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.algorithm_id, QStringLiteral("contour_mask"));
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("mask_heavy"));
        QCOMPARE(settings_value.algorithm_overlay_enabled, true);
    }
    QCOMPARE(
        preset_combo->currentData().toString(), QStringLiteral("mask_heavy")
    );

    algorithm_advanced_checkbox->setChecked(true);
    QVERIFY(!preset_combo->isHidden());
    QVERIFY(!movement_display_combo->isHidden());

    const int movement_off_index
        = movement_display_combo->findData(QStringLiteral("off"));
    QVERIFY(movement_off_index >= 0);
    movement_display_combo->setCurrentIndex(movement_off_index);

    QCOMPARE(stream_settings_spy.count(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("mask_heavy"));
        QCOMPARE(settings_value.algorithm_overlay_enabled, false);
    }
    QCOMPARE(
        operator_profile_combo->currentData().toString(),
        QStringLiteral("custom")
    );
}

void main_window_tests::settings_panel_exports_current_filtered_log_report() {
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
            .manual_core_fps = 9,
            .manual_processing_pixels = 320 * 240,
        }
    );

    const stream_settings settings_value = cell.current_stream_settings();
    QCOMPARE(settings_value.stream_name, QStringLiteral("cam-7"));
    QCOMPARE(settings_value.standard_labels_enabled, true);
    QCOMPARE(settings_value.algorithm_id, QStringLiteral("contour_mask"));
    QCOMPARE(settings_value.algorithm_preset, QStringLiteral("mask_heavy"));
    QCOMPARE(settings_value.algorithm_overlay_enabled, true);
    QVERIFY(settings_value.manual_processing_policy_enabled);
    QCOMPARE(settings_value.manual_display_fps, 17);
    QCOMPARE(settings_value.manual_core_fps, 9);
    QCOMPARE(settings_value.manual_processing_pixels, 320 * 240);

    cell.set_log_mode(app_log_mode::debug);
    QCOMPARE(cell.current_log_mode(), app_log_mode::debug);

    cell.set_runtime_metrics(
        stream_runtime_metrics {
            .input_fps = 29.7,
            .core_fps = 9.8,
            .input_width = 1280,
            .input_height = 720,
            .processed_width = 320,
            .processed_height = 180,
            .effective_display_fps = 17,
            .effective_core_fps = 9,
            .effective_processing_pixels = 320 * 180,
            .manual_policy_active = true,
            .processing_summary = {},
        }
    );
    const stream_runtime_metrics metrics = cell.current_runtime_metrics();
    QCOMPARE(metrics.processed_width, 320);
    QCOMPARE(metrics.processed_height, 180);
    QCOMPARE(metrics.effective_core_fps, 9);
    QVERIFY(metrics.manual_policy_active);
}

void main_window_tests::stream_cell_coalesces_frames_before_image_conversion() {
    stream_cell cell(QStringLiteral("cam-coalesced"));
    cell.set_repaint_interval_ms(10'000);
    QSignalSpy frame_spy(&cell, &stream_cell::frame_ready);
    QImage first_frame(QSize(64, 48), QImage::Format_ARGB32_Premultiplied);
    QImage second_frame(QSize(64, 48), QImage::Format_ARGB32_Premultiplied);
    first_frame.fill(Qt::black);
    second_frame.fill(Qt::white);

    QVERIFY(
        QMetaObject::invokeMethod(
            &cell, "on_frame_changed", Qt::DirectConnection,
            Q_ARG(QVideoFrame, QVideoFrame(first_frame))
        )
    );
    QVERIFY(
        QMetaObject::invokeMethod(
            &cell, "on_frame_changed", Qt::DirectConnection,
            Q_ARG(QVideoFrame, QVideoFrame(second_frame))
        )
    );

    QCOMPARE(frame_spy.count(), 1);
}

void main_window_tests::stream_cell_negative_auto_masks_underlying_content() {
    const QSize frame_size(240, 180);
    const QImage frame
        = main_window_tests_support::gradient_test_frame(frame_size);

    stream_cell::line_instance open_line;
    open_line.template_name = QStringLiteral("invert-line");
    open_line.color_mode_id = QStringLiteral("negative_auto");
    open_line.width_text = QStringLiteral("6");
    open_line.pts_pct = {
        QPointF(15.0, 25.0),
        QPointF(85.0, 25.0),
    };

    stream_cell::line_instance closed_line;
    closed_line.template_name = QStringLiteral("invert-area");
    closed_line.color_mode_id = QStringLiteral("negative_auto");
    closed_line.closed = true;
    closed_line.width_text = QStringLiteral("4");
    closed_line.pts_pct = {
        QPointF(25.0, 60.0),
        QPointF(75.0, 60.0),
        QPointF(75.0, 85.0),
        QPointF(25.0, 85.0),
    };

    const QImage idle_rendered
        = main_window_tests_support::render_stream_cell_with_frame(
            frame_size, frame, { open_line, closed_line }
        );
    const QImage rendered = main_window_tests_support::
        render_stream_cell_with_frame_and_highlights(
            frame_size, frame, { open_line, closed_line },
            {
                {
                    QStringLiteral("invert-line"),
                    QPointF(50.0, 25.0),
                },
                {
                    QStringLiteral("invert-area"),
                    QPointF(50.0, 72.5),
                },
            }
        );

    const QPoint line_sample_a(60, 45);
    const QPoint line_sample_b(180, 45);
    const QColor idle_a = idle_rendered.pixelColor(line_sample_a);
    const QColor idle_b = idle_rendered.pixelColor(line_sample_b);
    const QColor rendered_a = rendered.pixelColor(line_sample_a);
    const QColor rendered_b = rendered.pixelColor(line_sample_b);

    QVERIFY(rendered_a != idle_a);
    QVERIFY(rendered_b != idle_b);

    const QPoint fill_sample(120, 130);
    const QPoint outside_sample(20, 130);
    const QColor source_fill = frame.pixelColor(fill_sample);
    QCOMPARE(idle_rendered.pixelColor(fill_sample), source_fill);
    const QColor rendered_fill = rendered.pixelColor(fill_sample);
    const QColor inverted_fill(
        255 - source_fill.red(), 255 - source_fill.green(),
        255 - source_fill.blue()
    );

    QVERIFY(
        main_window_tests_support::color_close(rendered_fill, inverted_fill)
    );
    QCOMPARE(
        rendered.pixelColor(outside_sample), frame.pixelColor(outside_sample)
    );
}

void main_window_tests::stream_cell_emits_line_edit_interaction_signals() {
    stream_cell cell(QStringLiteral("cam-edit"));
    cell.resize(300, 200);
    cell.set_active(true);
    cell.show();
    QCoreApplication::processEvents();

    stream_cell::line_instance line_value;
    line_value.template_name = QStringLiteral("north_edit");
    line_value.color = QColor(QStringLiteral("#67c1ff"));
    line_value.color_mode_id = QStringLiteral("manual");
    line_value.enabled = true;
    line_value.closed = false;
    line_value.pts_pct = {
        QPointF(20.0, 70.0),
        QPointF(50.0, 50.0),
        QPointF(80.0, 30.0),
    };
    cell.set_line_edit_preview(line_value);

    QSignalSpy select_spy(&cell, &stream_cell::line_edit_point_selected);
    QSignalSpy shape_drag_spy(
        &cell, &stream_cell::line_edit_shape_drag_requested
    );
    QSignalSpy point_move_spy(
        &cell, &stream_cell::line_edit_point_move_requested
    );
    QSignalSpy split_spy(&cell, &stream_cell::line_edit_point_split_requested);
    QSignalSpy rotate_spy(
        &cell, &stream_cell::line_edit_shape_rotate_requested
    );

    const QPoint shape_anchor(105, 120);
    const QPoint middle_vertex(150, 100);
    const QPoint moved_shape(117, 132);
    const QPoint moved_vertex(168, 115);

    cell.setFocus();
    QVERIFY(cell.hasFocus());

    QTest::keyClick(&cell, Qt::Key_Right);
    QVERIFY(!shape_drag_spy.isEmpty());
    {
        const auto args = shape_drag_spy.takeLast();
        const QPointF delta_pct = args.at(0).toPointF();
        QVERIFY(std::abs(delta_pct.x() - (100.0 / 300.0)) < 0.05);
        QVERIFY(std::abs(delta_pct.y()) < 0.05);
    }

    QTest::mousePress(&cell, Qt::LeftButton, Qt::NoModifier, shape_anchor);
    {
        QMouseEvent move_event(
            QEvent::MouseMove, moved_shape, cell.mapToGlobal(moved_shape),
            Qt::NoButton, Qt::LeftButton, Qt::NoModifier
        );
        QApplication::sendEvent(&cell, &move_event);
    }
    QTest::mouseRelease(&cell, Qt::LeftButton, Qt::NoModifier, moved_shape);
    QVERIFY(!shape_drag_spy.isEmpty());
    {
        const auto args = shape_drag_spy.takeLast();
        const QPointF delta_pct = args.at(0).toPointF();
        QVERIFY(std::abs(delta_pct.x() - 4.0) < 0.25);
        QVERIFY(std::abs(delta_pct.y() - 6.0) < 0.25);
    }

    QTest::mouseClick(&cell, Qt::LeftButton, Qt::NoModifier, middle_vertex);
    QCOMPARE(select_spy.size(), 1);
    QCOMPARE(select_spy.takeLast().at(0).toInt(), 1);

    QTest::mousePress(&cell, Qt::LeftButton, Qt::NoModifier, middle_vertex);
    {
        QMouseEvent move_event(
            QEvent::MouseMove, moved_vertex, cell.mapToGlobal(moved_vertex),
            Qt::NoButton, Qt::LeftButton, Qt::NoModifier
        );
        QApplication::sendEvent(&cell, &move_event);
    }
    QTest::mouseRelease(&cell, Qt::LeftButton, Qt::NoModifier, moved_vertex);
    QVERIFY(!point_move_spy.isEmpty());
    {
        const auto args = point_move_spy.takeLast();
        QCOMPARE(args.at(0).toInt(), 1);
        const QPointF point_pct = args.at(1).toPointF();
        QVERIFY(std::abs(point_pct.x() - 56.0) < 0.25);
        QVERIFY(std::abs(point_pct.y() - 57.5) < 0.25);
    }

    QTest::keyClick(&cell, Qt::Key_Up);
    QVERIFY(!point_move_spy.isEmpty());
    {
        const auto args = point_move_spy.takeLast();
        QCOMPARE(args.at(0).toInt(), 1);
        const QPointF point_pct = args.at(1).toPointF();
        QVERIFY(std::abs(point_pct.x() - 50.0) < 0.05);
        QVERIFY(std::abs(point_pct.y() - 49.5) < 0.05);
    }

    {
        QWheelEvent wheel_event(
            QPointF(middle_vertex), QPointF(cell.mapToGlobal(middle_vertex)),
            QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
            Qt::NoScrollPhase, false
        );
        QApplication::sendEvent(&cell, &wheel_event);
    }
    QVERIFY(!rotate_spy.isEmpty());
    {
        const auto args = rotate_spy.takeLast();
        QVERIFY(std::abs(args.at(0).toDouble() - 5.0) < 0.05);
        QCOMPARE(args.at(1).toInt(), 1);
    }

    QTest::mouseDClick(&cell, Qt::LeftButton, Qt::NoModifier, middle_vertex);
    QVERIFY(!split_spy.isEmpty());
    QCOMPARE(split_spy.takeLast().at(0).toInt(), 1);
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
            balanced_status, app_log_mode::release
        );
    const QImage debug_status
        = main_window_tests_support::render_stream_cell_status_region(
            balanced_status, app_log_mode::debug
        );
    const QImage debug_heavy_algorithm_status
        = main_window_tests_support::render_stream_cell_status_region(
            debug_heavy_status, app_log_mode::debug
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

void main_window_tests::
    stream_cell_overlay_modes_render_different_event_regions() {
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
    const stream_settings track_overlay {
        .stream_name = QStringLiteral("cam-render"),
        .labels_enabled = true,
        .algorithm_id = QStringLiteral("centroid_track"),
        .algorithm_preset = QStringLiteral("persistent"),
        .algorithm_overlay_enabled = true,
    };

    const QImage no_overlay_image
        = main_window_tests_support::render_stream_cell_event_region(
            no_overlay
        );
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
    const QImage track_overlay_image
        = main_window_tests_support::render_stream_cell_event_region(
            track_overlay
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
    QVERIFY(
        main_window_tests_support::differing_pixel_count(
            contour_overlay_image, track_overlay_image
        )
        > 120
    );
}

void main_window_tests::
    stream_cell_line_profiles_render_different_wave_regions() {
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

void main_window_tests::
    configuration_import_export_round_trips_through_controller() {
    using namespace yodau::core;

    QTemporaryDir temporary_dir;
    QVERIFY(temporary_dir.isValid());
    const QString import_path
        = temporary_dir.filePath(QStringLiteral("import.yodau.json"));
    const QString export_path
        = temporary_dir.filePath(QStringLiteral("export.yodau.json"));

    line_configuration_document source_document;
    source_document.stream.name = "config-cam";
    source_document.stream.source = "/tmp/config-cam.mp4";
    source_document.stream.type = "file";
    source_document.stream.loop = false;
    source_document.stream.virtual_camera_path = "/dev/custom-yodau7";
    source_document.stream.analysis_interval_ms = 100;
    source_document.stream.algorithm
        = default_processing_algorithm_settings("spot_grid");
    source_document.stream.algorithm.preset_id = "dense";
    source_document.stream.labels_enabled = false;
    source_document.stream.standard_labels_enabled = false;
    source_document.stream.movement_display_mode = "bubbles";
    source_document.stream.manual_processing_policy_enabled = true;
    source_document.stream.manual_display_fps = 20;
    source_document.stream.manual_core_fps = 10;
    source_document.stream.manual_processing_pixels = 640 * 360;

    configured_line line_value;
    line_value.name = "door";
    line_value.points = {
        point { 10.0f, 25.0f },
        point { 90.0f, 25.0f },
    };
    line_value.enabled = false;
    line_value.direction = tripwire_dir::pos_to_neg;
    line_value.profile = make_line_profile("door", 3.0f, 4.0f, 1.5f, 0.75f);
    line_value.appearance = line_configuration_appearance {
        .color = "#ff336699",
        .color_mode = "manual",
        .width_text = "thick",
        .length_text = "long",
        .response_text = "resonant",
    };
    source_document.lines.push_back(std::move(line_value));
    save_line_configuration_atomic(
        source_document, std::filesystem::path(import_path.toStdString())
    );

    stream_manager manager;
    settings_panel settings;
    stream_board board;
    stream_controller controller(&manager, &settings, &board);
    controller.handle_add_file(
        QStringLiteral("/tmp/already-active.mp4"),
        QStringLiteral("already-active"), false
    );
    controller.handle_show_stream_changed(
        QStringLiteral("already-active"), true
    );
    board.grid_mode()->stream_enlarge(QStringLiteral("already-active"));
    QCOMPARE(
        controller.active_configuration_stream_name(),
        QStringLiteral("already-active")
    );

    QString error;
    QVERIFY2(
        controller.import_line_configuration_from(import_path, &error),
        qPrintable(error)
    );
    QVERIFY(manager.find_stream("config-cam") != nullptr);
    QVERIFY(manager.find_stream("already-active") != nullptr);
    QCOMPARE(manager.stream_lines("config-cam").size(), size_t(0));

    controller.handle_show_stream_changed(QStringLiteral("config-cam"), true);
    board.grid_mode()->stream_enlarge(QStringLiteral("config-cam"));
    QCOMPARE(
        controller.active_configuration_stream_name(),
        QStringLiteral("config-cam")
    );
    QVERIFY2(
        controller.export_line_configuration_to(export_path, &error),
        qPrintable(error)
    );

    const auto exported = load_line_configuration(
        std::filesystem::path(export_path.toStdString())
    );
    QVERIFY(exported.stream.name == "config-cam");
    QVERIFY(exported.stream.source == "/tmp/config-cam.mp4");
    QVERIFY(exported.stream.type == "file");
    QVERIFY(!exported.stream.loop);
    QVERIFY(exported.stream.virtual_camera_path == "/dev/custom-yodau7");
    QVERIFY(exported.stream.algorithm.algorithm_id == "spot_grid");
    QVERIFY(exported.stream.algorithm.preset_id == "dense");
    QVERIFY(!exported.stream.labels_enabled);
    QVERIFY(!exported.stream.standard_labels_enabled);
    QVERIFY(exported.stream.movement_display_mode == "bubbles");
    QVERIFY(exported.stream.manual_processing_policy_enabled);
    QCOMPARE(exported.stream.manual_display_fps, 20);
    QCOMPARE(exported.stream.manual_core_fps, 10);
    QCOMPARE(exported.stream.manual_processing_pixels, 640 * 360);
    QCOMPARE(exported.lines.size(), size_t(1));
    QVERIFY(exported.lines.front().name == "door");
    QVERIFY(!exported.lines.front().enabled);
    QVERIFY(exported.lines.front().direction == tripwire_dir::pos_to_neg);
    QVERIFY(exported.lines.front() == source_document.lines.front());
    QVERIFY(
        exported.lines.front().profile == source_document.lines.front().profile
    );
    QVERIFY(exported.lines.front().appearance.color == "#ff336699");
    QVERIFY(
        exported.lines.front().appearance
        == source_document.lines.front().appearance
    );

    QByteArray exported_data;
    QVERIFY2(
        controller.export_line_configuration_data(&exported_data, &error),
        qPrintable(error)
    );
    const auto exported_from_data = parse_line_configuration(
        std::string_view(
            exported_data.constData(), static_cast<size_t>(exported_data.size())
        )
    );
    QVERIFY(exported_from_data == exported);

    stream_manager data_manager;
    settings_panel data_settings;
    stream_board data_board;
    stream_controller data_controller(
        &data_manager, &data_settings, &data_board
    );
    QString imported_stream_name;
    QVERIFY2(
        data_controller.import_line_configuration_data(
            exported_data, &error, &imported_stream_name
        ),
        qPrintable(error)
    );
    QCOMPARE(imported_stream_name, QStringLiteral("config-cam"));
    QVERIFY(data_manager.find_stream("config-cam") != nullptr);
    QVERIFY(!data_controller.import_line_configuration_data(
        QByteArrayLiteral("{broken"), &error
    ));
    QVERIFY(!error.isEmpty());

    QVERIFY2(
        controller.import_line_configuration_from(export_path, &error),
        qPrintable(error)
    );
    QCOMPARE(manager.line_names().size(), size_t(1));
    QCOMPARE(manager.stream_lines("config-cam").size(), size_t(0));
}

void main_window_tests::
    app_log_bounds_history_and_updates_views_incrementally() {
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

void main_window_tests::
    stream_cell_exposes_keyboard_creation_and_accessible_controls() {
    stream_cell cell(QStringLiteral("keyboard-cam"));
    cell.resize(320, 240);
    cell.set_active(true);
    cell.set_drawing_enabled(true);
    cell.show();
    cell.setFocus();

    QTest::keyClick(&cell, Qt::Key_Space);
    QTest::keyClick(&cell, Qt::Key_Right, Qt::ShiftModifier);
    QTest::keyClick(&cell, Qt::Key_Space);
    const auto points = cell.draft_points_pct();
    QCOMPARE(points.size(), size_t(2));
    QVERIFY(points.at(1).x() > points.at(0).x());

    auto* focus_button = cell.findChild<QPushButton*>(
        QStringLiteral("stream_cell_focus_button")
    );
    auto* close_button = cell.findChild<QPushButton*>(
        QStringLiteral("stream_cell_close_button")
    );
    QVERIFY(focus_button != nullptr);
    QVERIFY(close_button != nullptr);
    QCOMPARE(focus_button->focusPolicy(), Qt::StrongFocus);
    QCOMPARE(close_button->focusPolicy(), Qt::StrongFocus);
    QVERIFY(!focus_button->accessibleName().isEmpty());
    QVERIFY(!close_button->accessibleName().isEmpty());
    QVERIFY(focus_button->minimumWidth() >= 32 || focus_button->width() >= 32);
    QVERIFY(!cell.accessibleDescription().isEmpty());

    QTest::keyClick(&cell, Qt::Key_Escape);
    QVERIFY(cell.draft_points_pct().empty());

    stream_source_panel source_panel;
    source_panel.set_existing_names({ QStringLiteral("taken") });
    auto* name_edit = source_panel.findChild<QLineEdit*>(
        QStringLiteral("settings_add_name_edit")
    );
    auto* error_label = source_panel.findChild<QLabel*>(
        QStringLiteral("settings_add_name_error_label")
    );
    QVERIFY(name_edit != nullptr);
    QVERIFY(error_label != nullptr);
    name_edit->setText(QStringLiteral("taken"));
    QVERIFY(
        error_label->isVisibleTo(&source_panel)
        || !error_label->text().isEmpty()
    );
    QVERIFY(!name_edit->accessibleDescription().isEmpty());
}

void main_window_tests::window_state_store_round_trips_shared_layout() {
    QTemporaryDir temporary_directory;
    QVERIFY(temporary_directory.isValid());
    QSettings settings(
        temporary_directory.filePath(QStringLiteral("desktop-state.ini")),
        QSettings::IniFormat
    );

    QMainWindow plain_window;
    auto* plain_dock
        = new QDockWidget(QStringLiteral("Streams"), &plain_window);
    plain_dock->setObjectName(QStringLiteral("main_window_streams_dock"));
    plain_window.addDockWidget(Qt::RightDockWidgetArea, plain_dock);
    auto* plain_toolbar = plain_window.addToolBar(QStringLiteral("Main"));
    plain_toolbar->setObjectName(QStringLiteral("main_toolbar"));
    plain_window.resize(640, 480);

    QString error;
    QVERIFY2(
        yodau::shell::save_main_window_state(plain_window, settings, &error),
        qPrintable(error)
    );

    QMainWindow kde_window;
    auto* kde_dock = new QDockWidget(QStringLiteral("Streams"), &kde_window);
    kde_dock->setObjectName(QStringLiteral("main_window_streams_dock"));
    kde_window.addDockWidget(Qt::LeftDockWidgetArea, kde_dock);
    auto* kde_toolbar = kde_window.addToolBar(QStringLiteral("Main"));
    kde_toolbar->setObjectName(QStringLiteral("main_toolbar"));

    QVERIFY2(
        yodau::shell::restore_main_window_state(kde_window, settings, &error),
        qPrintable(error)
    );
    QCOMPARE(kde_window.size(), plain_window.size());
    QCOMPARE(
        kde_window.dockWidgetArea(kde_dock),
        plain_window.dockWidgetArea(plain_dock)
    );

    kde_window.resize(600, 420);
    kde_window.addDockWidget(Qt::BottomDockWidgetArea, kde_dock);
    QVERIFY2(
        yodau::shell::save_main_window_state(kde_window, settings, &error),
        qPrintable(error)
    );

    QMainWindow restored_plain_window;
    auto* restored_plain_dock
        = new QDockWidget(QStringLiteral("Streams"), &restored_plain_window);
    restored_plain_dock->setObjectName(
        QStringLiteral("main_window_streams_dock")
    );
    restored_plain_window.addDockWidget(
        Qt::RightDockWidgetArea, restored_plain_dock
    );
    auto* restored_plain_toolbar
        = restored_plain_window.addToolBar(QStringLiteral("Main"));
    restored_plain_toolbar->setObjectName(QStringLiteral("main_toolbar"));

    QVERIFY2(
        yodau::shell::restore_main_window_state(
            restored_plain_window, settings, &error
        ),
        qPrintable(error)
    );
    QCOMPARE(restored_plain_window.size(), kde_window.size());
    QCOMPARE(
        restored_plain_window.dockWidgetArea(restored_plain_dock),
        Qt::BottomDockWidgetArea
    );
}

void main_window_tests::
    mobile_session_store_round_trips_and_rejects_unsafe_state() {
    using namespace yodau::core;
    using namespace yodau::shell;

    QTemporaryDir temporary_directory;
    QVERIFY(temporary_directory.isValid());
    const QString settings_path
        = temporary_directory.filePath(QStringLiteral("mobile-state.ini"));
    const QString configuration_path = temporary_directory.filePath(
        QStringLiteral("mobile-session.yodau.json")
    );
    QSettings settings(settings_path, QSettings::IniFormat);

    line_configuration_document document;
    document.stream.name = "mobile-camera";
    document.stream.source = "content://media/video/17";
    document.stream.type = "file";
    document.stream.algorithm
        = default_processing_algorithm_settings("motion_baseline");
    const std::string serialized = serialize_line_configuration(document);
    const QByteArray configuration(
        serialized.data(), static_cast<qsizetype>(serialized.size())
    );

    QString error;
    QVERIFY2(
        save_mobile_session(
            settings, configuration_path, mobile_page::lines, configuration,
            &error
        ),
        qPrintable(error)
    );
    const mobile_session_state restored
        = load_mobile_session(settings, configuration_path);
    QCOMPARE(restored.page, mobile_page::lines);
    QCOMPARE(restored.line_configuration, configuration);
    QVERIFY(restored.warning.isEmpty());

    QFile stored_configuration(configuration_path);
    QVERIFY(stored_configuration.open(QIODevice::ReadOnly));
    const QByteArray last_good_configuration = stored_configuration.readAll();
    stored_configuration.close();

    const QByteArray oversized(maximum_mobile_configuration_bytes + 1, 'x');
    QVERIFY(!save_mobile_session(
        settings, configuration_path, mobile_page::logs, oversized, &error
    ));
    QVERIFY(error.contains(QStringLiteral("4 MiB")));
    QVERIFY(stored_configuration.open(QIODevice::ReadOnly));
    QCOMPARE(stored_configuration.readAll(), last_good_configuration);
    stored_configuration.close();

    settings.setValue(QStringLiteral("mobile/session/schema_version"), 999);
    settings.sync();
    QVERIFY(!save_mobile_session(
        settings, configuration_path, mobile_page::monitor, configuration,
        &error
    ));
    QVERIFY(error.contains(QStringLiteral("newer")));
    QVERIFY(stored_configuration.open(QIODevice::ReadOnly));
    QCOMPARE(stored_configuration.readAll(), last_good_configuration);
    stored_configuration.close();
    QVERIFY(!clear_mobile_configuration(
        settings, configuration_path, mobile_page::monitor, &error
    ));
    QVERIFY(QFileInfo::exists(configuration_path));

    settings.setValue(
        QStringLiteral("mobile/session/schema_version"),
        QStringLiteral("not-a-version")
    );
    settings.sync();
    QVERIFY(!save_mobile_navigation(settings, mobile_page::monitor, &error));
    QVERIFY(error.contains(QStringLiteral("malformed")));
    const mobile_session_state malformed
        = load_mobile_session(settings, configuration_path);
    QVERIFY(malformed.line_configuration.isEmpty());
    QVERIFY(malformed.warning.contains(QStringLiteral("malformed")));
    QVERIFY(QFileInfo::exists(configuration_path));

    settings.setValue(
        QStringLiteral("mobile/session/schema_version"),
        mobile_session_schema_version
    );
    settings.sync();
    QVERIFY(
        stored_configuration.open(QIODevice::WriteOnly | QIODevice::Truncate)
    );
    QCOMPARE(stored_configuration.write("{broken"), qint64(7));
    stored_configuration.close();
    const mobile_session_state corrupt
        = load_mobile_session(settings, configuration_path);
    QVERIFY(corrupt.line_configuration.isEmpty());
    QVERIFY(!corrupt.warning.isEmpty());

    QVERIFY2(
        clear_mobile_configuration(
            settings, configuration_path, mobile_page::streams, &error
        ),
        qPrintable(error)
    );
    QVERIFY(!QFileInfo::exists(configuration_path));
    const mobile_session_state cleared
        = load_mobile_session(settings, configuration_path);
    QCOMPARE(cleared.page, mobile_page::streams);
    QVERIFY(cleared.line_configuration.isEmpty());
    QCOMPARE(normalized_mobile_page(99), mobile_page::monitor);
}

void main_window_tests::
    main_window_exposes_profile_shell_and_shared_workflows() {
    QTemporaryDir temporary_directory;
    QVERIFY(temporary_directory.isValid());
    const QByteArray previous_config_home = qgetenv("XDG_CONFIG_HOME");
    const auto restore_config_home = qScopeGuard([previous_config_home]() {
        if (previous_config_home.isNull()) {
            qunsetenv("XDG_CONFIG_HOME");
        } else {
            qputenv("XDG_CONFIG_HOME", previous_config_home);
        }
    });
    QVERIFY(qputenv("XDG_CONFIG_HOME", temporary_directory.path().toUtf8()));

    {
        main_window window;
#if !defined(KC_KDE)
        QVERIFY(
            window.findChild<QAction*>(
                QStringLiteral("main_import_line_configuration_action")
            )
            != nullptr
        );
        QVERIFY(
            window.findChild<QAction*>(
                QStringLiteral("main_export_line_configuration_action")
            )
            != nullptr
        );
#endif
#if !defined(KC_ANDROID) && !defined(Q_OS_ANDROID)
        QVERIFY(
            window.findChild<QDockWidget*>(
                QStringLiteral("main_window_streams_dock")
            )
            != nullptr
        );
        QVERIFY(
            window.findChild<QDockWidget*>(
                QStringLiteral("main_window_line_dock")
            )
            != nullptr
        );
        QVERIFY(
            window.findChild<QDockWidget*>(
                QStringLiteral("main_window_log_dock")
            )
            != nullptr
        );
#else
        QVERIFY(
            window.findChild<QScrollArea*>(
                QStringLiteral("mobile_streams_scroll")
            )
            != nullptr
        );
        QVERIFY(
            window.findChild<QScrollArea*>(
                QStringLiteral("mobile_lines_scroll")
            )
            != nullptr
        );
        auto* page_stack = window.findChild<QStackedWidget*>(
            QStringLiteral("mobile_page_stack")
        );
        QVERIFY(page_stack != nullptr);
        QCOMPARE(page_stack->count(), 4);
        auto* lines_action
            = window.findChild<QAction*>(QStringLiteral("mobile_navigation_2"));
        QVERIFY(lines_action != nullptr);
        lines_action->trigger();
        QCOMPARE(page_stack->currentIndex(), 2);
        QTest::keyClick(&window, Qt::Key_Back);
        QCOMPARE(page_stack->currentIndex(), 0);
#endif

#ifdef KC_KDE
        QVERIFY(
            window.actionCollection()->action(
                QStringLiteral("line_configuration_import")
            )
            != nullptr
        );
        QVERIFY(
            window.actionCollection()->action(
                QStringLiteral("line_configuration_export")
            )
            != nullptr
        );
        QVERIFY(
            window.actionCollection()->action(QStringLiteral("file_quit"))
            != nullptr
        );
        QVERIFY(
            window.actionCollection()->action(
                QStringLiteral("options_configure")
            )
            != nullptr
        );
#else
#if !defined(KC_ANDROID) && !defined(Q_OS_ANDROID)
        QVERIFY(
            window.findChild<QAction*>(QStringLiteral("main_quit_action"))
            != nullptr
        );
        QVERIFY(
            window.findChild<QAction*>(
                QStringLiteral("main_preferences_action")
            )
            != nullptr
        );
        QVERIFY(
            window.findChild<QAction*>(QStringLiteral("main_about_action"))
            != nullptr
        );
#endif
#endif
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
        QVERIFY(
            window.findChild<QToolBar*>(
                QStringLiteral("mobile_document_toolbar")
            )
            != nullptr
        );
        QVERIFY(
            window.findChild<QToolBar*>(
                QStringLiteral("mobile_navigation_toolbar")
            )
            != nullptr
        );
#else
        QVERIFY(
            window.findChild<QToolBar*>(QStringLiteral("main_toolbar"))
            != nullptr
        );
        QVERIFY(
            window.findChild<QToolBar*>(QStringLiteral("debug_toolbar"))
            != nullptr
        );
#endif
    }
}

void main_window_tests::window_state_store_rejects_incompatible_schema() {
    QTemporaryDir temporary_directory;
    QVERIFY(temporary_directory.isValid());
    QSettings settings(
        temporary_directory.filePath(QStringLiteral("desktop-state.ini")),
        QSettings::IniFormat
    );
    settings.setValue(
        QStringLiteral("desktop/main_window/schema_version"), 999
    );
    settings.setValue(
        QStringLiteral("desktop/main_window/geometry"), QByteArray("invalid")
    );
    settings.setValue(
        QStringLiteral("desktop/main_window/layout"), QByteArray("invalid")
    );
    settings.sync();

    QMainWindow window;
    QString error;
    QVERIFY(!yodau::shell::restore_main_window_state(window, settings, &error));
    QVERIFY(error.contains(QStringLiteral("schema version")));

    const QByteArray future_geometry
        = settings.value(QStringLiteral("desktop/main_window/geometry"))
              .toByteArray();
    const QByteArray future_layout
        = settings.value(QStringLiteral("desktop/main_window/layout"))
              .toByteArray();
    QVERIFY(!yodau::shell::save_main_window_state(window, settings, &error));
    QVERIFY(error.contains(QStringLiteral("not overwritten")));
    QCOMPARE(
        settings.value(QStringLiteral("desktop/main_window/schema_version"))
            .toInt(),
        999
    );
    QCOMPARE(
        settings.value(QStringLiteral("desktop/main_window/geometry"))
            .toByteArray(),
        future_geometry
    );
    QCOMPARE(
        settings.value(QStringLiteral("desktop/main_window/layout"))
            .toByteArray(),
        future_layout
    );
}

// NOLINTEND(readability-convert-member-functions-to-static)
