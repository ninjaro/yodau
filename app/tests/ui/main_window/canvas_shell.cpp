#include "ui/main_window_tests.hpp"

#include "shell/main_window.hpp"
#include "shell/window_state_store.hpp"
#include "widgets/active_editor_panel.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/settings_panel.hpp"
#include "widgets/stream_board.hpp"

#include <QAction>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QScopeGuard>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest/QtTest>

void main_window_tests::canvas_tools_preferences() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(
        directory.filePath("preferences.ini"), QSettings::IniFormat
    );
    settings.setValue("layout/stream_grid/preserve_order", true);
    const auto keys = settings.allKeys();
    QVERIFY(!yodau::shell::load_canvas_tools(settings));
    QCOMPARE(settings.allKeys(), keys); // Reading does not opt into anything.
    QVERIFY(yodau::shell::save_canvas_tools(settings, true));
    QVERIFY(yodau::shell::load_canvas_tools(settings));
    QVERIFY(!yodau::shell::load_desktop_presentation(settings).has_value());
    yodau::shell::desktop_presentation canvas;
    canvas.preset = yodau::shell::desktop_preset::canvas;
    QVERIFY(yodau::shell::save_desktop_presentation(settings, canvas));
    canvas.reset_overrides();
    QVERIFY(yodau::shell::save_desktop_presentation(settings, canvas));
    QVERIFY(yodau::shell::load_canvas_tools(settings));
    QVERIFY(settings.value("layout/stream_grid/preserve_order").toBool());
    QVERIFY(yodau::shell::save_canvas_tools(settings, false));
    QSettings reloaded(
        directory.filePath("preferences.ini"), QSettings::IniFormat
    );
    QVERIFY(!yodau::shell::load_canvas_tools(reloaded));
    // A directory cannot be an INI file. Failed writes must restore the cache.
    QSettings blocked(directory.path(), QSettings::IniFormat);
    QVERIFY(!yodau::shell::save_canvas_tools(blocked, true));
    QVERIFY(!blocked.contains("desktop/presentation/canvas_tools"));
    blocked.setValue("desktop/presentation/canvas_tools", false);
    QVERIFY(!yodau::shell::save_canvas_tools(blocked, true));
    QVERIFY(!yodau::shell::load_canvas_tools(blocked));
}

void main_window_tests::canvas_tools_shell() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto old_organization = QCoreApplication::organizationName();
    const auto old_application = QCoreApplication::applicationName();
    const auto old_format = QSettings::defaultFormat();
    const auto old_path = QFileInfo(QSettings(
                                        QSettings::IniFormat,
                                        QSettings::UserScope, "yodau_path_probe"
                                    )
                                        .fileName())
                              .path();
    const auto restore = qScopeGuard([&] {
        QCoreApplication::setOrganizationName(old_organization);
        QCoreApplication::setApplicationName(old_application);
        QSettings::setDefaultFormat(old_format);
        QSettings::setPath(
            QSettings::IniFormat, QSettings::UserScope, old_path
        );
    });
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(
        QSettings::IniFormat, QSettings::UserScope, directory.path()
    );
    QCoreApplication::setOrganizationName("yodau");
    QCoreApplication::setApplicationName("canvas-shell-tests");
    {
        main_window window;
        auto* action = window.findChild<QAction*>("desktop_canvas_tools");
        auto* tools = window.findChild<QWidget*>("canvas_tools");
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
        QVERIFY(action == nullptr && tools == nullptr);
#else
        QVERIFY(action && tools);
        QVERIFY(!action->isChecked());
        QVERIFY(tools->isHidden());
        auto* menu = window.findChild<QMenu*>("desktop_presentation_menu");
        QVERIFY(menu && menu->actions().contains(action));
        auto* board = window.findChild<stream_board*>();
        auto* settings = window.findChild<settings_panel*>();
        auto* lines = window.findChild<QDockWidget*>("main_window_line_dock");
        QVERIFY(board && settings && lines);
        QSettings preferences;
        action->trigger();
        QVERIFY(action->isChecked());
        QVERIFY(!tools->isHidden());
        QVERIFY(!tools->isVisible()); // No active canvas reserves no space.
        QVERIFY(!preferences.contains("desktop/presentation/preset"));
        QVERIFY(yodau::shell::load_canvas_tools(preferences));
        auto* canvas = window.findChild<QAction*>("desktop_preset_canvas");
        auto* reset = window.findChild<QAction*>("desktop_preset_reset");
        QVERIFY(canvas && reset);
        canvas->trigger();
        QVERIFY(lines->isHidden());
        reset->trigger();
        QVERIFY(action->isChecked() && !tools->isHidden());
        board->grid_mode()->add_stream("one");
        board->grid_mode()->add_stream("two");
        settings->set_active_candidates({ "one", "two" });
        settings->set_active_current("one");
        board->set_active_stream("one");
        window.resize(640, 480);
        window.show();
        QTRY_VERIFY(tools->isVisible());
        QVERIFY(board->grid_mode()->isVisible());
        auto* cell = board->active_cell();
        QTRY_VERIFY(
            tools->mapTo(board, tools->rect().bottomLeft()).y()
            < cell->mapTo(board, cell->rect().topLeft()).y()
        );
        QTRY_VERIFY(tools->width() <= board->width());
        for (const auto side :
             { pool_side::bottom, pool_side::left, pool_side::right }) {
            board->set_pool_side(side);
            QCoreApplication::processEvents();
            QVERIFY(board->grid_mode()->isVisible());
            QCOMPARE(board->active_cell(), cell);
        }
        // A failed visibility write leaves both live state and check state
        // intact.
        QFile blocker(directory.filePath("blocker"));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        QSettings::setPath(
            QSettings::IniFormat, QSettings::UserScope, blocker.fileName()
        );
        bool warned = false;
        QTimer::singleShot(0, &window, [&] {
            auto* dialog
                = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (dialog != nullptr) {
                warned = true;
                dialog->accept();
            }
        });
        action->trigger();
        QVERIFY(warned);
        QVERIFY(action->isChecked() && !tools->isHidden());
        QSettings::setPath(
            QSettings::IniFormat, QSettings::UserScope, directory.path()
        );
        lines->toggleViewAction()
            ->trigger(); // The original dock remains reachable.
        QVERIFY(!lines->isHidden());
#endif
    }
#if !defined(KC_ANDROID) && !defined(Q_OS_ANDROID)
    main_window restored;
    auto* action = restored.findChild<QAction*>("desktop_canvas_tools");
    auto* tools = restored.findChild<QWidget*>("canvas_tools");
    QVERIFY(action->isChecked() && !tools->isHidden());
    action->trigger();
    QVERIFY(!action->isChecked() && tools->isHidden());
#endif
}
