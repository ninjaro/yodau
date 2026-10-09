#include "ui/main_window_tests.hpp"

#include "widgets/file_transport.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/stream_board.hpp"
#include "widgets/stream_cell.hpp"

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QtTest/QtTest>

void main_window_tests::file_transport_states() {
    stream_board board;
    board.grid_mode()->add_stream("one");
    auto* cell = board.grid_mode()->peek_stream_cell("one");
    QVERIFY(cell);
    QVERIFY(board.findChild<file_transport*>() == nullptr);
    board.set_active_stream("one");
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QVERIFY(board.findChild<file_transport*>() == nullptr);
#else
    board.resize(360, 480);
    board.show();
    auto* controls = board.findChild<file_transport*>();
    QVERIFY(controls && controls->isHidden());
    auto* play = controls->findChild<QPushButton*>("file_play");
    auto* restart = controls->findChild<QPushButton*>("file_restart");
    auto* rate = controls->findChild<QComboBox*>("file_rate");
    auto* seek = controls->findChild<QSlider*>("file_seek");
    auto* status = controls->findChild<QLabel*>("file_status");
    QVERIFY(play && restart && rate && seek && status);
    QVERIFY(!seek->accessibleName().isEmpty());
    QVERIFY(!rate->accessibleName().isEmpty());
    QVERIFY(!seek->hasTracking());
    QCOMPARE(status->textFormat(), Qt::PlainText);
    QTemporaryDir missing;
    QVERIFY(missing.isValid());
    cell->set_source(QUrl::fromLocalFile(missing.filePath("absent.mp4")));
    QVERIFY(!controls->isHidden());
    QVERIFY(!seek->isEnabled());
    auto* player = cell->findChild<QMediaPlayer*>();
    QVERIFY(player);
    if (player->isAvailable()) {
        QTRY_VERIFY_WITH_TIMEOUT(
            player->error() != QMediaPlayer::NoError, 3000
        );
        QVERIFY(status->text().startsWith("File unavailable"));
    } else {
        QVERIFY(status->text().startsWith("File decoder unavailable"));
    }
    QVERIFY(!play->isEnabled() && !restart->isEnabled() && !rate->isEnabled());
    const auto count = board.findChildren<file_transport*>().size();
    cell->set_camera_id({}); // No physical camera is opened.
    QVERIFY(player->source().isEmpty());
    QVERIFY(controls->isHidden());
    cell->set_source(QUrl("qrc:/not-a-file"));
    QVERIFY(controls->isHidden());
    cell->set_source({});
    QVERIFY(controls->isHidden());
    QCOMPARE(board.findChildren<file_transport*>().size(), count);
#endif
}

void main_window_tests::file_transport_routing() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Desktop active-file transport only");
#else
    stream_board board;
    board.grid_mode()->add_stream("one");
    board.grid_mode()->add_stream("two");
    auto* one = board.grid_mode()->peek_stream_cell("one");
    auto* two = board.grid_mode()->peek_stream_cell("two");
    QVERIFY(one && two);
    board.set_active_stream("one");
    auto* controls = board.findChild<file_transport*>();
    QVERIFY(controls);
    one->set_source(QUrl::fromLocalFile("/tmp/unused-one.mp4"));
    one->findChild<QMediaPlayer*>()->setPlaybackRate(2.0);
    const auto points = std::vector<QPointF> { { 15, 30 }, { 85, 70 } };
    one->set_drawing_enabled(true);
    one->set_draft_points_pct(points);
    one->set_quarter_turns(1);
    one->set_keep_frame_aspect_ratio(true);
    board.set_pool_side(pool_side::right);
    board.set_active_stream("missing");
    QCOMPARE(board.active_cell(), one);
    QCOMPARE(one->draft_points_pct(), points);
    two->set_source(QUrl::fromLocalFile("/tmp/unused-two.mp4"));
    board.set_active_stream("two");
    QCOMPARE(board.active_cell(), two);
    QCOMPARE(board.findChildren<file_transport*>().size(), 1);
    auto* rate = controls->findChild<QComboBox*>("file_rate");
    QCOMPARE(rate->currentData().toDouble(), 1.0);
    one->findChild<QMediaPlayer*>()->setPlaybackRate(.5);
    QCOMPARE(rate->currentData().toDouble(), 1.0);
    QVERIFY(one->findChild<file_transport*>() == nullptr);
    board.set_active_stream("one");
    QCOMPARE(rate->currentData().toDouble(), .5);
    QCOMPARE(one->quarter_turns(), 1);
    QVERIFY(one->keeps_frame_aspect_ratio());
    board.clear_active();
    QVERIFY(controls->isHidden());
    board.set_active_stream("two");
    QCOMPARE(board.take_active_cell(), two);
    QVERIFY(controls->isHidden());
    two->deleteLater();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(controls->isHidden());
    board.set_active_stream("one");
    QVERIFY(!controls->isHidden());
    QCOMPARE(board.findChildren<file_transport*>().size(), 1);
#endif
}
