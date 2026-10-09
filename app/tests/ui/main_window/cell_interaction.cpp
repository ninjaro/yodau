#include "shell/app_settings.hpp"
#include "ui/main_window/fixtures.hpp"
#include "ui/main_window_tests.hpp"
#include "widgets/stream_cell.hpp"
#include "widgets/stream_source_panel.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest/QtTest>
#include <cmath>

// QtTest discovers these private slots through the meta-object.
// NOLINTBEGIN(readability-convert-member-functions-to-static)

void main_window_tests::cell_stream_settings() {
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

void main_window_tests::cell_coalesces_frame_conversion() {
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

void main_window_tests::cell_rotated_fitted_coordinates() {
    stream_cell cell(QStringLiteral("cam-transform"));
    cell.resize(400, 400);
    cell.set_active(true);
    cell.set_keep_frame_aspect_ratio(true);
    cell.set_quarter_turns(1);
    cell.set_drawing_enabled(true);
    cell.set_repaint_interval_ms(1);
    cell.show();

    QSignalSpy layout_spy(&cell, &stream_cell::layout_geometry_changed);
    QImage frame(QSize(200, 100), QImage::Format_ARGB32_Premultiplied);
    frame.fill(Qt::black);
    QVERIFY(
        QMetaObject::invokeMethod(
            &cell, "on_frame_changed", Qt::DirectConnection,
            Q_ARG(QVideoFrame, QVideoFrame(frame))
        )
    );
    QCOMPARE(cell.source_frame_aspect(), 2.0);
    QCOMPARE(cell.effective_display_aspect(), 0.5);
    QVERIFY(!layout_spy.isEmpty());

    layout_spy.clear();
    QTest::qWait(2);
    QVERIFY(
        QMetaObject::invokeMethod(
            &cell, "on_frame_changed", Qt::DirectConnection,
            Q_ARG(QVideoFrame, QVideoFrame(frame))
        )
    );
    QCOMPARE(layout_spy.count(), 0);

    // A 90-degree clockwise turn maps canonical (20, 30) to display
    // percentage (70, 20). The 2:1 source is fitted into a centered 200x400
    // rectangle, so this click lands at (240, 80).
    QTest::mouseClick(&cell, Qt::LeftButton, Qt::NoModifier, QPoint(240, 80));
    // A click in the left pillarbox is clamped to the display-frame edge
    // before the inverse rotation, producing canonical (20, 100).
    QTest::mouseClick(&cell, Qt::LeftButton, Qt::NoModifier, QPoint(0, 80));

    const auto points = cell.draft_points_pct();
    QCOMPARE(points.size(), size_t(2));
    QVERIFY(
        main_window_tests_support::point_close(
            points.at(0), QPointF(20.0, 30.0)
        )
    );
    QVERIFY(
        main_window_tests_support::point_close(
            points.at(1), QPointF(20.0, 100.0)
        )
    );
}

void main_window_tests::cell_edit_signals() {
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

void main_window_tests::cell_keyboard_and_accessibility() {
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

// NOLINTEND(readability-convert-member-functions-to-static)
