#ifndef YODAU_APP_TESTS_FILE_FIXTURE_HPP
#define YODAU_APP_TESTS_FILE_FIXTURE_HPP

#include "widgets/file_transport.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/stream_board.hpp"
#include "widgets/stream_cell.hpp"

#include <QComboBox>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace file_tests {

// Tiny synthetic video, not a checked-in binary or a dependency-wide benchmark.
inline bool create_clip(const QString& path) {
    QProcess process;
    process.start(
        QStandardPaths::findExecutable("ffmpeg"),
        {
            "-hide_banner",
            "-loglevel",
            "error",
            "-f",
            "lavfi",
            "-i",
            "testsrc=size=64x48:rate=12",
            "-t",
            "8",
            "-an",
            "-c:v",
            "mpeg4",
            "-threads",
            "1",
            "-filter_threads",
            "1",
            "-y",
            path,
        }
    );
    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished(1000);
        return false;
    }
    return process.exitStatus() == QProcess::NormalExit
        && process.exitCode() == 0;
}

struct fixture {
    QTemporaryDir files;
    stream_board board;
    stream_cell* cell { nullptr };
    QMediaPlayer* player { nullptr };
    file_transport* controls { nullptr };

    fixture() {
        board.grid_mode()->add_stream("sample");
        cell = board.grid_mode()->peek_stream_cell("sample");
        player = cell->findChild<QMediaPlayer*>();
        cell->set_loop(false);
        board.set_active_stream("sample");
        controls = board.findChild<file_transport*>();
        board.resize(360, 480);
        board.show();
    }

    void open() {
        cell->set_source(QUrl::fromLocalFile(files.filePath("sample.mp4")));
    }

    template <class T> T* control(const char* name) {
        return controls->findChild<T*>(QString::fromLatin1(name));
    }
};

inline void choose_rate(QComboBox* combo, double rate) {
    const int index = combo->findData(rate);
    combo->setCurrentIndex(index);
    QMetaObject::invokeMethod(combo, "activated", Q_ARG(int, index));
}

} // namespace file_tests

#endif // YODAU_APP_TESTS_FILE_FIXTURE_HPP
