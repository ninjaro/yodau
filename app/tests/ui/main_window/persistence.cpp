#include "configuration/line_configuration_file.hpp"
#include "configuration/line_configuration_json.hpp"
#include "shell/app_settings.hpp"
#include "shell/main_window.hpp"
#include "shell/mobile_session_store.hpp"
#include "ui/main_window_tests.hpp"

#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QToolBar>
#include <QtTest/QtTest>

// QtTest discovers these private slots through the meta-object.
// NOLINTBEGIN(readability-convert-member-functions-to-static)

void main_window_tests::window_state_round_trip() {
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

void main_window_tests::mobile_state_round_trip_and_validation() {
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
    const std::string serialized
        = yodau::core::encode_line_configuration_json(document);
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

void main_window_tests::window_state_rejects_invalid_payload() {
    QTemporaryDir temporary_directory;
    QVERIFY(temporary_directory.isValid());
    QSettings settings(
        temporary_directory.filePath(QStringLiteral("desktop-state.ini")),
        QSettings::IniFormat
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
    QVERIFY(!error.isEmpty());
    QVERIFY2(
        yodau::shell::save_main_window_state(window, settings, &error),
        qPrintable(error)
    );
}

// NOLINTEND(readability-convert-member-functions-to-static)
