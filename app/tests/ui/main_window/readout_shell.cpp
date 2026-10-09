#include "ui/main_window_tests.hpp"

#include "shell/main_window.hpp"
#include "shell/window_state_store.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/processing_readout.hpp"
#include "widgets/stream_board.hpp"

#include <QAction>
#include <QDockWidget>
#include <QFileInfo>
#include <QMenu>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest/QtTest>

#ifdef KC_KDE
#include <KAboutData>
#include <KActionCollection>
#endif

namespace {

struct preferences_scope {
    QTemporaryDir directory;
    QString organization = QCoreApplication::organizationName();
    QString application = QCoreApplication::applicationName();
    QSettings::Format format = QSettings::defaultFormat();
    QString path = QFileInfo(QSettings(
                                 QSettings::IniFormat, QSettings::UserScope,
                                 "yodau_path_probe"
                             )
                                 .fileName())
                       .path();

    preferences_scope() {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(
            QSettings::IniFormat, QSettings::UserScope, directory.path()
        );
        QCoreApplication::setOrganizationName("yodau");
#ifdef KC_KDE
        // XMLGUI consults cached KDE identity; isolate by path, not a
        // conflicting application name left over from another test's window.
        QCoreApplication::setApplicationName(
            KAboutData::applicationData().componentName()
        );
#else
        QCoreApplication::setApplicationName("readout-tests");
#endif
    }

    ~preferences_scope() {
        QCoreApplication::setOrganizationName(organization);
        QCoreApplication::setApplicationName(application);
        QSettings::setDefaultFormat(format);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, path);
    }
};

} // namespace

void main_window_tests::processing_readout_shell() {
    preferences_scope preferences;
    QVERIFY(preferences.directory.isValid());
    {
        main_window window;
        auto* dock
            = window.findChild<QDockWidget*>("main_window_processing_dock");
        auto* action = window.findChild<QAction*>("view_processing_dock");
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
        QVERIFY(!dock && !action);
        QVERIFY(window.findChild<processing_readout*>() == nullptr);
#else
        QVERIFY(dock && action);
        QVERIFY(dock->isHidden() && !action->isChecked());
        auto* timer = dock->findChild<QTimer*>("processing_refresh_timer");
        QVERIFY(timer && !timer->isActive());
        QCOMPARE(action, dock->toggleViewAction());
        QCOMPARE(
            action->shortcut(), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P)
        );
#ifdef KC_KDE
        QCOMPARE(
            window.actionCollection()->action("view_processing_dock"), action
        );
#endif
        bool in_menu = false;
        for (const auto* menu : window.findChildren<QMenu*>()) {
            in_menu = in_menu || menu->actions().contains(action);
        }
        QVERIFY(in_menu);
        auto* board = window.findChild<stream_board*>();
        QVERIFY(board);
        board->grid_mode()->add_stream("one");
        board->grid_mode()->add_stream("two");
        board->set_active_stream("one");
        auto* cell = board->active_cell();
        auto* thumbnail = board->grid_mode()->peek_stream_cell("two");
        window.resize(1024, 768);
        window.show();
        action->trigger();
        QTRY_VERIFY(dock->isVisible() && timer->isActive());
        QVERIFY(action->isChecked());
        for (const auto* name :
             { "desktop_preset_canvas", "desktop_preset_reset",
               "desktop_preset_classic" }) {
            auto* preset = window.findChild<QAction*>(name);
            QVERIFY(preset);
            preset->trigger();
            QVERIFY(dock->isVisible() && timer->isActive());
            QCOMPARE(board->active_cell(), cell);
            QCOMPARE(board->grid_mode()->peek_stream_cell("two"), thumbnail);
        }
        dock->close();
        QVERIFY(dock->isHidden() && !action->isChecked() && !timer->isActive());
        action->trigger();
        QTRY_VERIFY(dock->isVisible());
#endif
    }
#if !defined(KC_ANDROID) && !defined(Q_OS_ANDROID)
    main_window restored;
    auto* dock
        = restored.findChild<QDockWidget*>("main_window_processing_dock");
    auto* timer = dock->findChild<QTimer*>("processing_refresh_timer");
    QVERIFY(!dock->isHidden());
    QVERIFY(
        !timer->isActive()
    ); // Restored visibility is not yet a shown window.
    restored.show();
    QTRY_VERIFY(dock->isVisible() && timer->isActive());
    QVERIFY(dock->toggleViewAction()->isChecked());
#endif
}

void main_window_tests::processing_readout_old_layout() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Desktop layout only");
#else
    preferences_scope preferences;
    QVERIFY(preferences.directory.isValid());
    QMainWindow old;
    auto* streams = new QDockWidget("Streams", &old);
    streams->setObjectName("main_window_streams_dock");
    old.addDockWidget(Qt::RightDockWidgetArea, streams);
    auto* lines = new QDockWidget("Lines", &old);
    lines->setObjectName("main_window_line_dock");
    old.addDockWidget(Qt::LeftDockWidgetArea, lines);
    auto* logs = new QDockWidget("Logs", &old);
    logs->setObjectName("main_window_log_dock");
    old.addDockWidget(Qt::BottomDockWidgetArea, logs);
    lines->hide();
    QSettings settings;
    QVERIFY(yodau::shell::save_main_window_state(old, settings));
    QVERIFY(!settings.contains("desktop/presentation/preset"));
    main_window restored;
    QCOMPARE(QSettings().fileName(), settings.fileName());
    auto* dock
        = restored.findChild<QDockWidget*>("main_window_processing_dock");
    QVERIFY(dock && dock->isHidden());
    QVERIFY(!dock->findChild<QTimer*>("processing_refresh_timer")->isActive());
    QVERIFY(
        restored.findChild<QDockWidget*>("main_window_line_dock")->isHidden()
    );
    QCOMPARE(
        restored.dockWidgetArea(
            restored.findChild<QDockWidget*>("main_window_log_dock")
        ),
        Qt::BottomDockWidgetArea
    );
    QVERIFY(!settings.contains("desktop/presentation/preset"));
#endif
}
