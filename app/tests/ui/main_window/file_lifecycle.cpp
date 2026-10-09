#include "ui/file_fixture.hpp"
#include "ui/main_window_tests.hpp"

#include <QLabel>
#include <QSlider>
#include <QtTest/QtTest>

void main_window_tests::file_lifecycle() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Desktop transport lifecycle only");
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
    file_tests::choose_rate(rate, .5);
    f.board.set_application_active(false);
    QTRY_COMPARE(f.player->playbackState(), QMediaPlayer::PausedState);
    QVERIFY(!play->isEnabled() && !rate->isEnabled());
    QVERIFY(f.control<QLabel>("file_status")->text().contains("inactive"));
    file_tests::choose_rate(rate, 2);
    QCOMPARE(f.player->playbackRate(), .5);
    f.board.set_application_active(true);
    QTRY_COMPARE(f.player->playbackState(), QMediaPlayer::PlayingState);
    QCOMPARE(rate->currentData().toDouble(), .5);
    play->click();
    QTRY_COMPARE(f.player->playbackState(), QMediaPlayer::PausedState);
    f.board.set_application_active(false);
    f.cell->set_loop(true);
    QVERIFY(
        QMetaObject::invokeMethod(
            f.cell, "on_media_status_changed",
            Q_ARG(QMediaPlayer::MediaStatus, QMediaPlayer::EndOfMedia)
        )
    );
    f.board.set_application_active(true);
    QCOMPARE(f.player->playbackState(), QMediaPlayer::PausedState);
    f.board.set_application_active(false);
    f.cell->set_camera_id("no-camera-is-opened-while-inactive");
    QVERIFY(f.controls->isHidden());
    QVERIFY(f.player->source().isEmpty());
    f.open(); // Replaces camera intent; activation cannot reopen the old
              // camera.
    QCOMPARE(f.player->playbackRate(), 1.0);
    f.board.set_application_active(true);
    QTRY_VERIFY_WITH_TIMEOUT(play->isEnabled(), 5000);
    QVERIFY(f.cell->findChild<QCamera*>() == nullptr);
#endif
}

void main_window_tests::file_loop() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Desktop file loop lifecycle only");
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
    QTRY_VERIFY_WITH_TIMEOUT(
        f.control<QSlider>("file_seek")->isEnabled(), 5000
    );
    f.player->pause();
    f.player->setPosition(f.player->duration() - 100);
    f.player->play();
    QTRY_COMPARE_WITH_TIMEOUT(
        f.player->mediaStatus(), QMediaPlayer::EndOfMedia, 3000
    );
    QCOMPARE(f.control<QLabel>("file_status")->text(), QString("Finished"));
    f.cell->set_loop(true);
    f.player->setPosition(f.player->duration() - 100);
    f.player->play();
    QSignalSpy status(f.player, &QMediaPlayer::mediaStatusChanged);
    QTRY_VERIFY_WITH_TIMEOUT(!status.isEmpty(), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(f.player->position() < 1000, 3000);
    QCOMPARE(f.player->playbackState(), QMediaPlayer::PlayingState);
    f.board.set_application_active(false);
    QVERIFY(
        QMetaObject::invokeMethod(
            f.cell, "on_media_status_changed",
            Q_ARG(QMediaPlayer::MediaStatus, QMediaPlayer::EndOfMedia)
        )
    );
    QVERIFY(f.player->playbackState() != QMediaPlayer::PlayingState);
    f.board.set_application_active(true);
    QTRY_COMPARE(f.player->playbackState(), QMediaPlayer::PlayingState);
#endif
}
