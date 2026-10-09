#include "ui/file_fixture.hpp"
#include "ui/main_window_tests.hpp"

#include <QLabel>
#include <QMouseEvent>
#include <QSlider>
#include <QStyleOptionSlider>
#include <QtTest/QtTest>

void main_window_tests::file_playback() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Desktop playback controls only");
#else
    if (QStandardPaths::findExecutable("ffmpeg").isEmpty()) {
        QSKIP("ffmpeg is required to generate the tiny media fixture");
    }
    file_tests::fixture f;
    if (!f.player->isAvailable()) {
        QSKIP("No Qt multimedia backend");
    }
    QVERIFY(file_tests::create_clip(f.files.filePath("sample.mp4")));
    f.open();
    auto* play = f.control<QPushButton>("file_play");
    auto* rate = f.control<QComboBox>("file_rate");
    QTRY_VERIFY_WITH_TIMEOUT(play->isEnabled(), 5000);
    QCOMPARE(f.player->playbackState(), QMediaPlayer::PlayingState);
    play->setFocus();
    QTest::keyClick(play, Qt::Key_Space);
    QTRY_COMPARE(f.player->playbackState(), QMediaPlayer::PausedState);
    QCOMPARE(play->text(), QString("Play"));
    const auto points = std::vector<QPointF> { { 10, 20 }, { 80, 70 } };
    f.cell->set_drawing_enabled(true);
    f.cell->set_draft_points_pct(points);
    f.cell->set_quarter_turns(1);
    for (const double speed : { .25, .5, 1., 1.5, 2. }) {
        file_tests::choose_rate(rate, speed);
        QCOMPARE(f.player->playbackRate(), speed);
        QCOMPARE(rate->currentData().toDouble(), f.player->playbackRate());
    }
    QCOMPARE(f.cell->draft_points_pct(), points);
    QCOMPARE(f.cell->quarter_turns(), 1);
    QSignalSpy frames(f.cell, &stream_cell::frame_ready);
    QTest::keyClick(play, Qt::Key_Space);
    QTRY_COMPARE(f.player->playbackState(), QMediaPlayer::PlayingState);
    QTRY_VERIFY_WITH_TIMEOUT(!frames.isEmpty(), 3000);
    f.board.clear_active();
    QCOMPARE(f.player->playbackRate(), 2.0);
    f.board.set_active_stream("sample");
    QCOMPARE(f.board.findChild<file_transport*>(), f.controls);
    QCOMPARE(rate->currentData().toDouble(), 2.0);
#endif
}

void main_window_tests::file_seek() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Desktop playback controls only");
#else
    if (QStandardPaths::findExecutable("ffmpeg").isEmpty()) {
        QSKIP("ffmpeg is required to generate the tiny media fixture");
    }
    file_tests::fixture f;
    if (!f.player->isAvailable()) {
        QSKIP("No Qt multimedia backend");
    }
    QVERIFY(file_tests::create_clip(f.files.filePath("sample.mp4")));
    f.open();
    auto* seek = f.control<QSlider>("file_seek");
    QTRY_VERIFY_WITH_TIMEOUT(seek->isEnabled(), 5000);
    f.control<QPushButton>("file_play")->click();
    QTRY_COMPARE(f.player->playbackState(), QMediaPlayer::PausedState);
    QCOMPARE(f.player->duration(), qint64(8000));
    seek->setFocus();
    QTest::keyClick(seek, Qt::Key_Home);
    QTRY_COMPARE(f.player->position(), qint64(0));
    QTest::keyClick(seek, Qt::Key_PageUp);
    QTRY_VERIFY(f.player->position() > 0);
    const auto previous = f.player->position();
    seek->setSliderDown(true);
    seek->setSliderPosition(seek->maximum() / 2);
    QCOMPARE(f.player->position(), previous);
    QTest::keyClick(seek, Qt::Key_Escape);
    QVERIFY(!seek->isSliderDown());
    QCOMPARE(f.player->position(), previous);

    // Cancellation must also survive move/release from a real pressed handle.
    QStyleOptionSlider option;
    option.initFrom(seek);
    option.orientation = Qt::Horizontal;
    option.minimum = seek->minimum();
    option.maximum = seek->maximum();
    option.sliderPosition = seek->sliderPosition();
    option.sliderValue = seek->value();
    const auto handle = seek->style()->subControlRect(
        QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, seek
    );
    QTest::mousePress(seek, Qt::LeftButton, Qt::NoModifier, handle.center());
    QVERIFY(seek->isSliderDown());
    const auto target = seek->rect().center();
    const auto move = [&] {
        QMouseEvent event(
            QEvent::MouseMove, target, seek->mapToGlobal(target), Qt::NoButton,
            Qt::LeftButton, Qt::NoModifier
        );
        QApplication::sendEvent(seek, &event);
    };
    move();
    QCOMPARE(f.player->position(), previous);
    QTest::keyClick(seek, Qt::Key_Escape);
    move();
    QTest::mouseRelease(seek, Qt::LeftButton, Qt::NoModifier, target);
    QCOMPARE(f.player->position(), previous);

    QTest::mousePress(seek, Qt::LeftButton, Qt::NoModifier, handle.center());
    move();
    QCOMPARE(f.player->position(), previous);
    QTest::mouseRelease(seek, Qt::LeftButton, Qt::NoModifier, target);
    QTRY_VERIFY(f.player->position() > 3000 && f.player->position() < 5000);

    seek->setSliderDown(true);
    seek->setSliderPosition(seek->maximum() / 2);
    seek->setSliderDown(false);
    QTRY_COMPARE(f.player->position(), qint64(4000));
    QVERIFY(f.control<QLabel>("file_time")->text().contains("00:00:04"));
    QTest::keyClick(seek, Qt::Key_End);
    QTRY_COMPARE(f.player->position(), f.player->duration());
    f.control<QPushButton>("file_restart")->click();
    QTRY_COMPARE(f.player->playbackState(), QMediaPlayer::PlayingState);
    QVERIFY(f.player->position() < 1000);
#endif
}
