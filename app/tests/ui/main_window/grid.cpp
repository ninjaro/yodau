#include "shell/app_settings.hpp"
#include "ui/main_window_tests.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/stream_board.hpp"
#include "widgets/stream_cell.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QMessageBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QtTest/QtTest>

#include <algorithm>

// QtTest discovers these private slots through the meta-object.
// NOLINTBEGIN(readability-convert-member-functions-to-static)

void main_window_tests::grid_packing_plans() {
    grid_view view;
    view.resize(960, 420);
    view.show();
    view.add_stream(QStringLiteral("camera-d"));
    view.add_stream(QStringLiteral("camera-a"));
    view.add_stream(QStringLiteral("camera-c"));
    view.add_stream(QStringLiteral("camera-b"));

    QTRY_COMPARE(view.layout_row_count(), 2);
    QTRY_COMPARE(view.layout_column_count(), 2);
    QCOMPARE(view.layout_cell_count(), 4);
    QCOMPARE(
        view.stream_names(),
        QStringList(
            { QStringLiteral("camera-a"), QStringLiteral("camera-b"),
              QStringLiteral("camera-c"), QStringLiteral("camera-d") }
        )
    );

    view.resize(960, 210);
    QTRY_COMPARE(view.layout_row_count(), 1);
    QTRY_COMPARE(view.layout_column_count(), 4);
    QCOMPARE(view.layout_cell_count(), 4);

    view.remove_stream(QStringLiteral("camera-b"));
    QTRY_COMPARE(view.layout_cell_count(), 3);

    view.set_preserve_stream_order(true);
    QVERIFY(view.preserves_stream_order());
    QTRY_COMPARE(view.layout_cell_count(), 3);

    view.set_scroll_direction(grid_view::scroll_direction::vertical);
    QCOMPARE(
        view.current_scroll_direction(), grid_view::scroll_direction::vertical
    );
    QTRY_COMPARE(view.layout_cell_count(), 3);

    view.set_sizing_mode(grid_view::sizing_mode::fixed);
    QCOMPARE(view.current_sizing_mode(), grid_view::sizing_mode::fixed);
    QTRY_COMPARE(view.layout_cell_count(), 3);
    for (const QString& stream_name : view.stream_names()) {
        const stream_cell* cell = view.peek_stream_cell(stream_name);
        QVERIFY(cell != nullptr);
        QVERIFY(cell->has_manual_short_side());
        QVERIFY(cell->manual_short_side() >= 96.0);
    }
}

void main_window_tests::grid_session_sizes_and_viewport_changes() {
    grid_view view;
    view.resize(960, 210);
    view.show();
    view.add_stream(QStringLiteral("camera-a"));
    view.add_stream(QStringLiteral("camera-b"));
    view.add_stream(QStringLiteral("camera-c"));
    view.add_stream(QStringLiteral("camera-d"));

    auto* scroll = view.findChild<QScrollArea*>(
        QStringLiteral("stream_grid_scroll_area")
    );
    QVERIFY(scroll != nullptr);
    QTRY_VERIFY(scroll->horizontalScrollBar()->isVisible());
    QTRY_VERIFY(([&view, scroll]() {
        for (const QString& stream_name : view.stream_names()) {
            const stream_cell* cell = view.peek_stream_cell(stream_name);
            if (cell == nullptr
                || cell->geometry().bottom() >= scroll->viewport()->height()) {
                return false;
            }
        }
        return true;
    }()));

    view.set_sizing_mode(grid_view::sizing_mode::fixed);
    stream_cell* first = view.peek_stream_cell(QStringLiteral("camera-a"));
    QVERIFY(first != nullptr);
    first->initialize_manual_short_side(222.0);
    view.set_sizing_mode(grid_view::sizing_mode::adaptive);
    view.hide();

    bool restore_question_seen = false;
    QTimer::singleShot(0, [&restore_question_seen]() {
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            auto* question = qobject_cast<QMessageBox*>(widget);
            if (question != nullptr) {
                restore_question_seen = true;
                question->done(QMessageBox::Yes);
                return;
            }
        }
    });
    view.set_sizing_mode(grid_view::sizing_mode::fixed);
    QVERIFY(restore_question_seen);
    QCOMPARE(first->manual_short_side(), 222.0);

    view.show();
    first->initialize_manual_short_side(600.0);
    first->set_quarter_turns(1);
    QTRY_VERIFY(scroll->verticalScrollBar()->isVisible());
    QTRY_VERIFY(
        std::min(first->width(), first->height())
        >= qRound(first->manual_short_side())
    );
}

void main_window_tests::board_keeps_active_for_stale_selection() {
    stream_board board;
    board.resize(800, 600);
    auto* grid = board.grid_mode();
    grid->add_stream(QStringLiteral("first"));
    grid->add_stream(QStringLiteral("second"));
    auto* first = grid->peek_stream_cell(QStringLiteral("first"));
    auto* second = grid->peek_stream_cell(QStringLiteral("second"));
    QVERIFY(first != nullptr);
    QVERIFY(second != nullptr);
    board.set_active_stream(QStringLiteral("missing"));
    QVERIFY(board.active_cell() == nullptr);
    QCOMPARE(grid->stream_names().size(), 2);

    board.set_active_stream(QStringLiteral("first"));
    board.show();
    QCoreApplication::processEvents();
    QVERIFY(first->isVisibleTo(&board));
    QVERIFY(grid->isVisibleTo(&board));
    for (const auto& name :
         { QString(), QStringLiteral("first"), QStringLiteral("missing") }) {
        board.set_active_stream(name);
        QCOMPARE(board.active_cell(), first);
        QVERIFY(first->is_active());
        QVERIFY(first->isVisibleTo(&board));
        QVERIFY(grid->isVisibleTo(&board));
        QCOMPARE(
            grid->stream_names(), QStringList { QStringLiteral("second") }
        );
        QCOMPARE(grid->peek_stream_cell(QStringLiteral("second")), second);
        QCOMPARE(board.findChildren<stream_cell*>().size(), 2);
    }

    board.set_active_stream(QStringLiteral("second"));
    QCOMPARE(board.active_cell(), second);
    QVERIFY(second->is_active());
    QVERIFY(!first->is_active());
    QCOMPARE(grid->peek_stream_cell(QStringLiteral("first")), first);
    QVERIFY(first->isVisibleTo(&board));
    QVERIFY(grid->isVisibleTo(&board));
    board.clear_active();
    QVERIFY(board.active_cell() == nullptr);
    QCOMPARE(grid->stream_names().size(), 2);
    QCOMPARE(grid->peek_stream_cell(QStringLiteral("second")), second);
    QVERIFY(!second->is_active());
    QVERIFY(grid->isVisibleTo(&board));
}

// NOLINTEND(readability-convert-member-functions-to-static)
