#include "shell/app_settings.hpp"
#include "ui/main_window_tests.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/stream_board.hpp"
#include "widgets/stream_cell.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QImage>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest/QtTest>

#include <algorithm>

// QtTest discovers these private slots through the meta-object.
// NOLINTBEGIN(readability-convert-member-functions-to-static)

namespace thumbnail_tests {

void mouse_event(
    QWidget* grip, QEvent::Type type, const QPointF& global_pos,
    Qt::MouseButton button, Qt::MouseButtons buttons
) {
    const QPointF local = grip->mapFromGlobal(global_pos.toPoint());
    QMouseEvent event(type, local, global_pos, button, buttons, Qt::NoModifier);
    QApplication::sendEvent(grip, &event);
}

QWidget* grip_for(stream_cell& cell) {
    return cell.findChild<QWidget*>(
        QStringLiteral("stream_cell_resize_handle")
    );
}

QPointF size_vector(const stream_cell& cell) {
    const double aspect = cell.effective_display_aspect();
    return { std::max(1.0, aspect), std::max(1.0, 1.0 / aspect) };
}

} // namespace thumbnail_tests

void main_window_tests::thumbnail_drag_data() {
    QTest::addColumn<QSize>("frame_size");
    QTest::addColumn<int>("turns");
    QTest::addColumn<bool>("rtl");
    for (const auto size : { QSize(60, 40), QSize(80, 40), QSize(40, 80) }) {
        for (int turns = 0; turns < 4; ++turns) {
            for (const bool rtl : { false, true }) {
                const auto row = QStringLiteral("%1-%2-%3-%4")
                                     .arg(size.width())
                                     .arg(size.height())
                                     .arg(turns)
                                     .arg(rtl)
                                     .toLatin1();
                QTest::newRow(row.constData()) << size << turns << rtl;
            }
        }
    }
}

void main_window_tests::thumbnail_drag() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Corner grip is desktop-only");
#else
    using namespace thumbnail_tests;
    QFETCH(QSize, frame_size);
    QFETCH(int, turns);
    QFETCH(bool, rtl);
    stream_cell cell(QStringLiteral("camera"));
    QImage image(frame_size, QImage::Format_RGB32);
    image.fill(Qt::black);
    const QVideoFrame frame(image);
    QVERIFY(
        QMetaObject::invokeMethod(
            &cell, "on_frame_changed", Qt::DirectConnection,
            Q_ARG(QVideoFrame, frame)
        )
    );
    QCOMPARE(
        cell.source_frame_aspect(),
        static_cast<double>(frame_size.width()) / frame_size.height()
    );
    cell.set_quarter_turns(turns);
    cell.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
    cell.set_fixed_size_controls_visible(true);
    cell.initialize_manual_short_side(192.5);
    cell.resize(400, 300);
    cell.show();
    QCoreApplication::processEvents();
    auto* grip = grip_for(cell);
    QVERIFY(grip != nullptr);
    QVERIFY(grip->isVisibleTo(&cell));
    QCOMPARE(grip->geometry().right(), cell.width() - 2);
    QCOMPARE(grip->geometry().bottom(), cell.height() - 2);
    QCOMPARE(grip->cursor().shape(), Qt::SizeFDiagCursor);
    QVERIFY(!grip->accessibleName().isEmpty());
    QVERIFY(!grip->accessibleDescription().isEmpty());
    QSignalSpy layout_changes(&cell, &stream_cell::layout_geometry_changed);
    const QPointF origin = grip->mapToGlobal(grip->rect().center());
    const auto direction = size_vector(cell);
    mouse_event(
        grip, QEvent::MouseButtonPress, origin, Qt::RightButton, Qt::RightButton
    );
    mouse_event(
        grip, QEvent::MouseButtonRelease, origin, Qt::RightButton, Qt::NoButton
    );
    QCOMPARE(layout_changes.count(), 0);
    mouse_event(
        grip, QEvent::MouseButtonPress, origin, Qt::LeftButton, Qt::LeftButton
    );
    // A tangential move must not change aspect-constrained size.
    mouse_event(
        grip, QEvent::MouseMove,
        origin + QPointF(direction.y(), -direction.x()) * 16, Qt::NoButton,
        Qt::LeftButton
    );
    QCOMPARE(cell.manual_short_side(), 192.5);
    QCOMPARE(layout_changes.count(), 0);
    const QPointF moved = origin + direction * 32;
    mouse_event(grip, QEvent::MouseMove, moved, Qt::NoButton, Qt::LeftButton);
    QCOMPARE(cell.manual_short_side(), 224.5);
    QCOMPARE(layout_changes.count(), 1);
    cell.move(cell.pos() + QPoint(100, 80));
    cell.resize(420, 320);
    mouse_event(grip, QEvent::MouseMove, moved, Qt::NoButton, Qt::LeftButton);
    QCOMPARE(cell.manual_short_side(), 224.5);
    QCOMPARE(layout_changes.count(), 1);
    // Release position is consumed even without another move event.
    mouse_event(
        grip, QEvent::MouseButtonRelease, origin + direction * 48,
        Qt::LeftButton, Qt::NoButton
    );
    QCOMPARE(cell.manual_short_side(), 240.5);
    QCOMPARE(layout_changes.count(), 2);
    QVERIFY(QWidget::mouseGrabber() != grip);
    QCOMPARE(cell.size(), QSize(420, 320)); // Only Packing owns widget sizing.
    QCOMPARE(cell.quarter_turns(), turns);
#endif
}

void main_window_tests::thumbnail_cancel_data() {
    QTest::addColumn<QString>("cause");
    for (const auto* cause :
         { "escape", "ungrab", "hide", "hide_grip", "focus", "window",
           "disable", "mode", "active", "rotate", "reparent", "idle",
           "no_buttons", "aspect", "uninitialized" }) {
        QTest::newRow(cause) << QString::fromLatin1(cause);
    }
}

void main_window_tests::thumbnail_cancel() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Corner grip is desktop-only");
#else
    using namespace thumbnail_tests;
    QFETCH(QString, cause);
    QWidget new_parent;
    stream_cell cell(QStringLiteral("camera"));
    cell.set_fixed_size_controls_visible(true);
    if (cause != QStringLiteral("uninitialized")) {
        cell.initialize_manual_short_side(192.0);
    }
    const double start = cell.manual_short_side();
    const bool initialized = cell.has_manual_short_side();
    cell.resize(400, 300);
    cell.show();
    QCoreApplication::processEvents();
    auto* grip = grip_for(cell);
    QVERIFY(grip != nullptr);
    const QPointF origin = grip->mapToGlobal(grip->rect().center());
    mouse_event(
        grip, QEvent::MouseButtonPress, origin, Qt::LeftButton, Qt::LeftButton
    );
    const QPointF moved = origin + size_vector(cell) * 32;
    mouse_event(grip, QEvent::MouseMove, moved, Qt::NoButton, Qt::LeftButton);
    QCOMPARE(cell.manual_short_side(), start + 32);
    if (cause == QStringLiteral("escape")
        || cause == QStringLiteral("uninitialized")) {
        QTest::keyClick(&cell, Qt::Key_Escape);
    } else if (cause == QStringLiteral("ungrab")) {
        QEvent lost(QEvent::UngrabMouse);
        QApplication::sendEvent(grip, &lost);
    } else if (cause == QStringLiteral("hide")) {
        cell.hide();
    } else if (cause == QStringLiteral("hide_grip")) {
        grip->hide();
        QVERIFY(grip->isHidden());
    } else if (
        cause == QStringLiteral("focus") || cause == QStringLiteral("window")
    ) {
        QEvent lost(
            cause == QStringLiteral("focus") ? QEvent::FocusOut
                                             : QEvent::WindowDeactivate
        );
        QApplication::sendEvent(&cell, &lost);
    } else if (cause == QStringLiteral("disable")) {
        cell.setEnabled(false);
    } else if (cause == QStringLiteral("mode")) {
        cell.set_fixed_size_controls_visible(false);
    } else if (cause == QStringLiteral("active")) {
        cell.set_active(true);
    } else if (cause == QStringLiteral("rotate")) {
        cell.set_quarter_turns(1);
    } else if (cause == QStringLiteral("reparent")) {
        cell.setParent(&new_parent);
    } else if (cause == QStringLiteral("idle")) {
        cell.set_application_active(false);
    } else if (cause == QStringLiteral("no_buttons")) {
        mouse_event(grip, QEvent::MouseMove, moved, Qt::NoButton, Qt::NoButton);
    } else if (cause == QStringLiteral("aspect")) {
        QImage image(QSize(40, 80), QImage::Format_RGB32);
        image.fill(Qt::black);
        const QVideoFrame frame(image);
        QVERIFY(
            QMetaObject::invokeMethod(
                &cell, "on_frame_changed", Qt::DirectConnection,
                Q_ARG(QVideoFrame, frame)
            )
        );
    }
    QCOMPARE(cell.manual_short_side(), start);
    QCOMPARE(cell.has_manual_short_side(), initialized);
    QVERIFY(QWidget::mouseGrabber() != grip);
    mouse_event(
        grip, QEvent::MouseButtonRelease, moved, Qt::LeftButton, Qt::NoButton
    );
    QCOMPARE(cell.manual_short_side(), start);
#endif
}

void main_window_tests::thumbnail_controls() {
    stream_cell cell(QStringLiteral("camera"));
    cell.resize(400, 300);
    cell.show();
    QCoreApplication::processEvents();
    auto* smaller = cell.findChild<QPushButton*>(
        QStringLiteral("stream_cell_size_decrease_button")
    );
    auto* larger = cell.findChild<QPushButton*>(
        QStringLiteral("stream_cell_size_increase_button")
    );
    QVERIFY(smaller && larger);
    auto* grip = thumbnail_tests::grip_for(cell);
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QVERIFY(grip == nullptr);
#else
    QVERIFY(grip != nullptr);
    QVERIFY(grip->isHidden());
#endif
    cell.set_fixed_size_controls_visible(true);
    cell.initialize_manual_short_side(96.0);
    QVERIFY(smaller->isVisibleTo(&cell));
    QVERIFY(larger->isVisibleTo(&cell));
    QVERIFY(!smaller->isEnabled());
    QVERIFY(!smaller->accessibleName().isEmpty());
    QVERIFY(!larger->accessibleName().isEmpty());
    QTest::keyClick(larger, Qt::Key_Space);
    QCOMPARE(cell.manual_short_side(), 112.0);
    QTest::keyClick(smaller, Qt::Key_Space);
    QCOMPARE(cell.manual_short_side(), 96.0);
#if !defined(KC_ANDROID) && !defined(Q_OS_ANDROID)
    const QPointF origin = grip->mapToGlobal(grip->rect().center());
    thumbnail_tests::mouse_event(
        grip, QEvent::MouseButtonPress, origin, Qt::LeftButton, Qt::LeftButton
    );
    thumbnail_tests::mouse_event(
        grip, QEvent::MouseButtonRelease,
        origin - thumbnail_tests::size_vector(cell) * 1000, Qt::LeftButton,
        Qt::NoButton
    );
    QCOMPARE(cell.manual_short_side(), 96.0);
    QVERIFY(!smaller->isEnabled());
#endif
    cell.set_active(true);
    cell.set_drawing_enabled(true);
    QVERIFY(smaller->isHidden() && larger->isHidden());
    if (grip) {
        QVERIFY(grip->isHidden());
    }
    const auto size = cell.manual_short_side();
    QTest::mouseClick(
        &cell, Qt::LeftButton, Qt::NoModifier,
        QPoint(cell.width() - 4, cell.height() - 4)
    );
    QCOMPARE(cell.draft_points_pct().size(), std::size_t(1));
    QCOMPARE(cell.manual_short_side(), size);
    cell.set_active(false);
    cell.set_fixed_size_controls_visible(false);
    if (grip) {
        QVERIFY(grip->isHidden());
    }
}

void main_window_tests::thumbnail_pool_data() {
    QTest::addColumn<int>("side");
    QTest::addColumn<bool>("vertical");
    QTest::addColumn<bool>("ordered");
    for (const auto side :
         { pool_side::bottom, pool_side::right, pool_side::left }) {
        for (const bool vertical : { false, true }) {
            for (const bool ordered : { false, true }) {
                const auto row = QStringLiteral("%1-%2-%3")
                                     .arg(static_cast<int>(side))
                                     .arg(vertical)
                                     .arg(ordered)
                                     .toLatin1();
                QTest::newRow(row.constData())
                    << static_cast<int>(side) << vertical << ordered;
            }
        }
    }
}

void main_window_tests::thumbnail_pool() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Corner grip is desktop-only");
#else
    using namespace thumbnail_tests;
    QFETCH(int, side);
    QFETCH(bool, vertical);
    QFETCH(bool, ordered);
    stream_board board;
    board.resize(1000, 700);
    board.set_pool_side(static_cast<pool_side>(side));
    auto* grid = board.grid_mode();
    grid->set_sizing_mode(grid_view::sizing_mode::fixed);
    grid->set_scroll_direction(
        vertical ? grid_view::scroll_direction::vertical
                 : grid_view::scroll_direction::horizontal
    );
    grid->set_preserve_stream_order(ordered);
    grid->add_stream(QStringLiteral("active"));
    grid->add_stream(QStringLiteral("first"));
    grid->add_stream(QStringLiteral("second"));
    board.set_active_stream(QStringLiteral("active"));
    auto* active = board.active_cell();
    auto* first = grid->peek_stream_cell(QStringLiteral("first"));
    auto* second = grid->peek_stream_cell(QStringLiteral("second"));
    QVERIFY(active && first && second);
    active->set_draft_points_pct({ QPointF(10, 20), QPointF(80, 90) });
    const auto points = active->draft_points_pct();
    board.show();
    QCoreApplication::processEvents();
    auto* grip = grip_for(*first);
    QVERIFY(grip != nullptr);
    QVERIFY(grip->isVisibleTo(&board));
    auto* scroll = grid->findChild<QScrollArea*>();
    QVERIFY(scroll != nullptr);
    scroll->ensureWidgetVisible(grip);
    QCoreApplication::processEvents();
    QTRY_VERIFY(scroll->viewport()->rect().contains(
        grip->mapTo(scroll->viewport(), grip->rect().center())
    ));
    const QPointF origin = grip->mapToGlobal(grip->rect().center());
    const auto direction = size_vector(*first);
    const double start = first->manual_short_side();
    mouse_event(
        grip, QEvent::MouseButtonPress, origin, Qt::LeftButton, Qt::LeftButton
    );
    mouse_event(
        grip, QEvent::MouseMove, origin + direction * 300, Qt::NoButton,
        Qt::LeftButton
    );
    QCOMPARE(first->manual_short_side(), start + 300);
    QCOMPARE(std::min(first->width(), first->height()), qRound(start + 300));
    mouse_event(
        grip, QEvent::MouseMove, origin + direction * 300, Qt::NoButton,
        Qt::LeftButton
    );
    QCOMPARE(first->manual_short_side(), start + 300);
    mouse_event(
        grip, QEvent::MouseButtonRelease, origin + direction * 320,
        Qt::LeftButton, Qt::NoButton
    );
    QCOMPARE(first->manual_short_side(), start + 320);
    QCOMPARE(std::min(first->width(), first->height()), qRound(start + 320));
    QCOMPARE(board.active_cell(), active);
    QCOMPARE(active->draft_points_pct(), points);
    QCOMPARE(grid->peek_stream_cell(QStringLiteral("second")), second);
    QCOMPARE(second->manual_short_side(), 160.0);
    QCOMPARE(board.current_pool_side(), static_cast<pool_side>(side));
    QCOMPARE(grid->preserves_stream_order(), ordered);
    board.resize(800, 500);
    QCoreApplication::processEvents();
    QCOMPARE(first->manual_short_side(), start + 320);
    QCOMPARE(std::min(first->width(), first->height()), qRound(start + 320));
    grid->set_sizing_mode(grid_view::sizing_mode::adaptive);
    QVERIFY(grip->isHidden());
    QTimer::singleShot(0, [] {
        for (auto* widget : QApplication::topLevelWidgets()) {
            if (auto* dialog = qobject_cast<QMessageBox*>(widget)) {
                dialog->done(QMessageBox::Yes);
            }
        }
    });
    grid->set_sizing_mode(grid_view::sizing_mode::fixed);
    QCOMPARE(first->manual_short_side(), start + 320);
    QVERIFY(grip->isVisibleTo(&board));
    QCOMPARE(std::min(first->width(), first->height()), qRound(start + 320));
#endif
}

// NOLINTEND(readability-convert-member-functions-to-static)
