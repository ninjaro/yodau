#include "shell/app_settings.hpp"
#include "shell/main_window.hpp"
#include "ui/main_window_tests.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/stream_board.hpp"
#include "widgets/stream_cell.hpp"

#include <QCoreApplication>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QtTest>

// QtTest discovers these private slots through the meta-object.
// NOLINTBEGIN(readability-convert-member-functions-to-static)

void main_window_tests::pool_placement_data() {
    QTest::addColumn<int>("side");
    QTest::addColumn<QSize>("size");
    QTest::addColumn<int>("count");
    QTest::addColumn<bool>("rtl");
    QTest::addColumn<bool>("vertical_scroll");
    for (const auto side :
         { pool_side::bottom, pool_side::right, pool_side::left }) {
        for (const auto size : { QSize(640, 480), QSize(1280, 800) }) {
            for (const int count : { 0, 1, 8 }) {
                for (const bool rtl : { false, true }) {
                    for (const bool vertical : { false, true }) {
                        const auto row = QStringLiteral("%1-%2-%3-%4-%5")
                                             .arg(static_cast<int>(side))
                                             .arg(size.width())
                                             .arg(count)
                                             .arg(rtl)
                                             .arg(vertical)
                                             .toLatin1();
                        QTest::newRow(row.constData())
                            << static_cast<int>(side) << size << count << rtl
                            << vertical;
                    }
                }
            }
        }
    }
}

void main_window_tests::pool_placement() {
    QFETCH(int, side);
    QFETCH(QSize, size);
    QFETCH(int, count);
    QFETCH(bool, rtl);
    QFETCH(bool, vertical_scroll);
    stream_board board;
    board.resize(size);
    auto* grid = board.grid_mode();
    const auto direction = vertical_scroll
        ? grid_view::scroll_direction::vertical
        : grid_view::scroll_direction::horizontal;
    grid->set_scroll_direction(direction);
    grid->add_stream(QStringLiteral("active"));
    for (int index = 0; index < count; ++index) {
        grid->add_stream(QStringLiteral("pool_%1").arg(index));
    }
    board.set_active_stream(QStringLiteral("active"));
    // Change text direction after side selection as well as at startup.
    const auto choice = static_cast<pool_side>(side);
    board.set_pool_side(choice);
    board.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
    board.show();
    QCoreApplication::processEvents();
    board.resize(size + QSize(160, 120));
    QCoreApplication::processEvents();
    board.resize(size);
    QCoreApplication::processEvents();
    QCOMPARE(board.size(), size);
    QCOMPARE(board.current_pool_side(), choice);
    QCOMPARE(grid->current_scroll_direction(), direction);
    auto* active = board.active_cell()->parentWidget();
    QVERIFY(active->isVisibleTo(&board));
    QVERIFY(board.rect().contains(active->geometry()));
    if (count == 0) {
        QVERIFY(grid->isHidden());
        QCOMPARE(active->geometry(), board.rect());
        return;
    }
    QVERIFY(grid->isVisibleTo(&board));
    QVERIFY(board.rect().contains(grid->geometry()));
    QVERIFY(!grid->geometry().intersects(active->geometry()));
    QVERIFY(grid->width() > 0 && grid->height() > 0);
    if (choice == pool_side::bottom) {
        QVERIFY(grid->y() > active->geometry().bottom());
        QVERIFY(active->height() > grid->height());
    } else {
        QVERIFY(active->width() > grid->width());
        if (choice == pool_side::left) {
            QVERIFY(grid->geometry().right() < active->x());
        } else {
            QVERIFY(grid->x() > active->geometry().right());
        }
    }
    // Overflow is reachable, not clipped to an inaccessible thumbnail area.
    auto* scroll = grid->findChild<QScrollArea*>();
    QVERIFY(scroll != nullptr);
    auto* bar = vertical_scroll ? scroll->verticalScrollBar()
                                : scroll->horizontalScrollBar();
    QTRY_COMPARE(bar->isVisibleTo(&board), bar->maximum() > 0);
    bar->setValue(bar->maximum());
    if (bar->maximum() > 0) {
        QCOMPARE(bar->value(), bar->maximum());
        QVERIFY(bar->isVisibleTo(&board));
    }
    QCOMPARE(grid->stream_names().size(), count);
}

void main_window_tests::pool_changes_preserve_cells() {
    stream_board board;
    board.resize(800, 600);
    auto* grid = board.grid_mode();
    grid->set_sizing_mode(grid_view::sizing_mode::fixed);
    grid->set_preserve_stream_order(true);
    grid->add_stream(QStringLiteral("first"));
    grid->add_stream(QStringLiteral("second"));
    auto* first = grid->peek_stream_cell(QStringLiteral("first"));
    auto* second = grid->peek_stream_cell(QStringLiteral("second"));
    second->initialize_manual_short_side(192.0);
    first->set_quarter_turns(1);
    first->set_keep_frame_aspect_ratio(false);
    first->set_draft_points_pct({ QPointF(10, 20), QPointF(80, 90) });
    const auto points = first->draft_points_pct();
    auto* player = first->findChild<QMediaPlayer*>();
    QVERIFY(player != nullptr);
    board.set_active_stream(QStringLiteral("first"));
    board.show();
    QCoreApplication::processEvents();
    first->setFocus();
    QTRY_VERIFY(first->hasFocus());
    for (const auto side :
         { pool_side::left, pool_side::right, pool_side::bottom }) {
        board.set_pool_side(side);
        QCoreApplication::processEvents();
        QCOMPARE(board.active_cell(), first);
        QCOMPARE(grid->peek_stream_cell(QStringLiteral("second")), second);
        QCOMPARE(board.findChildren<stream_cell*>().size(), 2);
        QCOMPARE(first->findChild<QMediaPlayer*>(), player);
        QCOMPARE(first->draft_points_pct(), points);
        QCOMPARE(first->quarter_turns(), 1);
        QVERIFY(!first->keeps_frame_aspect_ratio());
        QCOMPARE(second->manual_short_side(), 192.0);
        QCOMPARE(grid->current_sizing_mode(), grid_view::sizing_mode::fixed);
        QVERIFY(grid->preserves_stream_order());
        QVERIFY(first->hasFocus());
        board.set_active_stream(QStringLiteral("missing"));
        QCOMPARE(board.active_cell(), first);
    }
    board.set_pool_side(pool_side::left);
    board.set_pool_side(static_cast<pool_side>(99));
    QCOMPARE(board.current_pool_side(), pool_side::left);
    board.hide();
    board.set_pool_side(pool_side::right);
    board.show();
    QCoreApplication::processEvents();
    QVERIFY(first->isVisibleTo(&board));
    QVERIFY(grid->isVisibleTo(&board));
    QCOMPARE(board.take_active_cell(), first);
    grid->put_stream_cell(first);
    QCoreApplication::processEvents();
    QCOMPARE(grid->geometry(), board.rect());
    board.set_active_stream(QStringLiteral("second"));
    board.clear_active();
    QCoreApplication::processEvents();
    QCOMPARE(grid->geometry(), board.rect());
    grid->remove_stream(QStringLiteral("first"));
    grid->remove_stream(QStringLiteral("second"));
    board.set_pool_side(pool_side::left);
    QVERIFY(grid->isHidden());
    QVERIFY(board.active_cell() == nullptr);
    grid->add_stream(QStringLiteral("replacement"));
    QCoreApplication::processEvents();
    QCOMPARE(grid->geometry(), board.rect());
}

void main_window_tests::pool_preference_is_independent() {
    using namespace yodau::shell;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("pool.ini"));
    QSettings settings(path, QSettings::IniFormat);
    settings.setValue(
        QStringLiteral("desktop/main_window/layout"), QByteArray("layout")
    );
    settings.setValue(
        QStringLiteral("layout/stream_grid/scroll_direction"),
        QStringLiteral("vertical")
    );
    settings.setValue(QStringLiteral("mobile/session"), QByteArray("session"));
    const auto old_keys = settings.allKeys();
    QCOMPARE(load_pool_side(settings), pool_side::bottom);
    QCOMPARE(settings.allKeys(), old_keys);
    for (const auto side :
         { pool_side::right, pool_side::left, pool_side::bottom }) {
        QVERIFY(save_pool_side(settings, side));
        QSettings reloaded(path, QSettings::IniFormat);
        QCOMPARE(load_pool_side(reloaded), side);
        QVERIFY(!load_desktop_presentation(reloaded));
        desktop_presentation value;
        value.preset = desktop_preset::canvas;
        QVERIFY(save_desktop_presentation(settings, value));
        value.reset_overrides();
        QVERIFY(save_desktop_presentation(settings, value));
        QCOMPARE(load_pool_side(settings), side);
        settings.remove(QStringLiteral("desktop/presentation/preset"));
    }
    QCOMPARE(
        settings.value(QStringLiteral("desktop/main_window/layout"))
            .toByteArray(),
        QByteArray("layout")
    );
    QCOMPARE(
        settings.value(QStringLiteral("layout/stream_grid/scroll_direction"))
            .toString(),
        QStringLiteral("vertical")
    );
    QCOMPARE(
        settings.value(QStringLiteral("mobile/session")).toByteArray(),
        QByteArray("session")
    );
    const auto previous
        = settings.value(QStringLiteral("desktop/presentation/pool_side"));
    QVERIFY(!save_pool_side(settings, static_cast<pool_side>(99)));
    QCOMPARE(
        settings.value(QStringLiteral("desktop/presentation/pool_side")),
        previous
    );
    settings.setValue(
        QStringLiteral("desktop/presentation/pool_side"),
        QStringLiteral("future")
    );
    QCOMPARE(load_pool_side(settings), pool_side::bottom);
    QCOMPARE(
        settings.value(QStringLiteral("desktop/presentation/pool_side"))
            .toString(),
        QStringLiteral("future")
    );
    QSettings unwritable(directory.path(), QSettings::IniFormat);
    QVERIFY(!save_pool_side(unwritable, pool_side::left));
    QVERIFY(
        !unwritable.contains(QStringLiteral("desktop/presentation/pool_side"))
    );
    unwritable.setValue(
        QStringLiteral("desktop/presentation/pool_side"),
        QStringLiteral("right")
    );
    QVERIFY(!save_pool_side(unwritable, pool_side::left));
    QCOMPARE(load_pool_side(unwritable), pool_side::right);
}

// NOLINTEND(readability-convert-member-functions-to-static)
