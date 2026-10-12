#include "shell/desktop_shell.hpp"
#include "shell/main_window.hpp"
#include "ui/main_window_tests.hpp"

#include <QAction>
#include <QDialog>
#include <QDockWidget>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMenuBar>
#include <QScopeGuard>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
#include <QtTest/QtTest>

#ifdef KC_KDE
#include <KActionCollection>
#include <KCommandBar>
#include <KConfigGroup>
#include <KHelpMenu>
#include <KSharedConfig>
#include <KShortcutsDialog>
#endif

namespace {

struct desktop_fixture {
    base_main_window window;
    QAction* import = new QAction(QStringLiteral("Import"), &window);
    QAction* export_action = new QAction(QStringLiteral("Export"), &window);
    QMenu* presentation = new QMenu(QStringLiteral("Presentation"), &window);
    std::array<QDockWidget*, 4> docks {};
    int preferences_requests = 0;

    desktop_fixture() {
        window.setAttribute(Qt::WA_DeleteOnClose, false);
        window.resize(800, 600);
        window.setCentralWidget(new QWidget(&window));
        import->setObjectName(
            QStringLiteral("main_import_line_configuration_action")
        );
        export_action->setObjectName(
            QStringLiteral("main_export_line_configuration_action")
        );
        import->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I));
        export_action->setShortcut(
            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E)
        );
        const std::array names { "main_window_streams_dock",
                                 "main_window_line_dock",
                                 "main_window_log_dock",
                                 "main_window_processing_dock" };
        for (std::size_t index = 0; index < docks.size(); ++index) {
            docks[index]
                = new QDockWidget(QString::fromLatin1(names[index]), &window);
            docks[index]->setObjectName(QString::fromLatin1(names[index]));
            docks[index]->setWidget(new QWidget(docks[index]));
            window.addDockWidget(Qt::RightDockWidgetArea, docks[index]);
        }
        auto* readout = docks[3]->toggleViewAction();
        readout->setObjectName(QStringLiteral("view_processing_dock"));
#ifdef KC_KDE
        KActionCollection::setDefaultShortcut(
            readout, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P)
        );
#else
        readout->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));
#endif
        yodau::shell::setup_desktop_shell(
            window, { import, export_action, presentation, docks }, [this] {
                ++preferences_requests;
                docks[0]->show();
            }
        );
    }
};

} // namespace

void main_window_tests::desktop_shell_menus_and_shortcuts() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Desktop menus are excluded from the Android shell");
#else
    desktop_fixture fixture;
    auto& window = fixture.window;
    QVERIFY(!QFile::exists(QStringLiteral(":/yodau/yodauui.rc")));
    QCOMPARE(window.menuBar()->actions().size(), 4);
    auto* file = window.findChild<QMenu*>(QStringLiteral("file"));
    auto* view = window.findChild<QMenu*>(QStringLiteral("view"));
    auto* settings = window.findChild<QMenu*>(QStringLiteral("settings"));
    auto* help = window.findChild<QMenu*>(QStringLiteral("help"));
    QVERIFY(file && view && settings && help);
    QCOMPARE(
        window.menuBar()->actions(),
        QList<QAction*>({ file->menuAction(), view->menuAction(),
                          settings->menuAction(), help->menuAction() })
    );
    QCOMPARE(file->actions()[0], fixture.import);
    QCOMPARE(file->actions()[1], fixture.export_action);
    QCOMPARE(view->actions()[0], fixture.presentation->menuAction());
    auto* toolbar = window.findChild<QToolBar*>(QStringLiteral("main_toolbar"));
    QVERIFY(toolbar != nullptr);
    QCOMPARE(toolbar->actions().size(), 3);
    QJsonObject snapshot;
    for (auto* menu : { file, view, settings, help }) {
        QJsonArray names;
        for (auto* action : menu->actions()) {
            if (!action->isSeparator()) {
                names.append(action->objectName());
            }
        }
        snapshot.insert(menu->objectName(), names);
    }
    QJsonArray buttons;
    for (auto* action : toolbar->actions()) {
        buttons.append(action->objectName());
    }
    snapshot.insert(QStringLiteral("main_toolbar"), buttons);
    qInfo().noquote() << "SHELL_SNAPSHOT"
                      << QJsonDocument(snapshot).toJson(QJsonDocument::Compact);
    for (std::size_t index = 0; index < fixture.docks.size(); ++index) {
        auto* toggle = fixture.docks[index]->toggleViewAction();
        QCOMPARE(view->actions()[static_cast<qsizetype>(index) + 1], toggle);
        if (index < 3) {
            QCOMPARE(toolbar->actions()[static_cast<qsizetype>(index)], toggle);
            QVERIFY(toolbar->actions().contains(toggle));
        }
    }
    QCOMPARE(
        fixture.import->shortcut(),
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I)
    );
    QCOMPARE(
        fixture.export_action->shortcut(),
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E)
    );
    QCOMPARE(
        fixture.docks[3]->toggleViewAction()->shortcut(),
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P)
    );
    window.show();
    window.activateWindow();
    QTRY_VERIFY(window.isActiveWindow());
    fixture.docks[1]->toggleViewAction()->trigger();
    QVERIFY(fixture.docks[1]->isHidden());
    fixture.docks[1]->toggleViewAction()->trigger();
    QVERIFY(!fixture.docks[1]->isHidden());

    QSignalSpy imports(fixture.import, &QAction::triggered);
    QSignalSpy exports(fixture.export_action, &QAction::triggered);
    QTest::keyClick(
        &window, Qt::Key_I, Qt::ControlModifier | Qt::ShiftModifier
    );
    QTest::keyClick(
        &window, Qt::Key_E, Qt::ControlModifier | Qt::ShiftModifier
    );
    QCOMPARE(imports.size(), 1);
    QCOMPARE(exports.size(), 1);

#ifdef KC_KDE
    auto* collection = window.findChild<KActionCollection*>(
        QStringLiteral("desktop_action_collection")
    );
    QVERIFY(collection != nullptr);
    QVERIFY(collection->parentGUIClient() == nullptr);
    QCOMPARE(collection->configGroup(), QStringLiteral("Shortcuts"));
    QCOMPARE(
        collection->action(QStringLiteral("line_configuration_import")),
        fixture.import
    );
    QCOMPARE(
        collection->action(QStringLiteral("view_presentation")),
        fixture.presentation->menuAction()
    );
    for (const auto* name :
         { "file_quit", "options_configure", "options_show_statusbar",
           "main_toolbar", "options_configure_keybinding", "open_kcommand_bar",
           "help_contents", "help_whats_this", "help_about_app",
           "help_about_kde" }) {
        QVERIFY2(
            collection->action(QString::fromLatin1(name)) != nullptr, name
        );
    }
    QVERIFY(
        collection->action(QStringLiteral("options_configure_toolbars"))
        == nullptr
    );
    collection->action(QStringLiteral("options_configure"))->trigger();
    QCOMPARE(fixture.preferences_requests, 1);
    toolbar->hide();
    collection->action(QStringLiteral("main_toolbar"))->trigger();
    QVERIFY(!toolbar->isHidden());
    collection->action(QStringLiteral("options_configure_keybinding"))
        ->trigger();
    auto* dialog = window.findChild<KShortcutsDialog*>();
    QVERIFY(dialog != nullptr);
    QVERIFY(dialog->actionCollections().contains(collection));
    dialog->reject();
    QCoreApplication::processEvents();
    window.activateWindow();
    QTRY_VERIFY(window.isActiveWindow());
    QTest::keyClick(&window, Qt::Key_I, Qt::ControlModifier | Qt::AltModifier);
    auto* bar
        = window.findChild<KCommandBar*>(QStringLiteral("desktop_command_bar"));
    QVERIFY(bar != nullptr);
    bar->hide();
    auto* help_provider = window.findChild<KHelpMenu*>();
    QVERIFY(help_provider != nullptr);
    QCOMPARE(
        collection->action(QStringLiteral("help_about_app")),
        help_provider->action(KHelpMenu::menuAboutApp)
    );
    QVERIFY(collection->action(QStringLiteral("file_quit"))
                ->shortcuts()
                .contains(QKeySequence(Qt::CTRL | Qt::Key_Q)));

    KConfigGroup shortcuts(
        KSharedConfig::openConfig(), QStringLiteral("Shortcuts")
    );
    const bool existed = shortcuts.hasKey("line_configuration_import");
    const auto previous
        = shortcuts.readEntry("line_configuration_import", QString());
    const auto restore_shortcut = qScopeGuard([&] {
        if (existed) {
            shortcuts.writeEntry("line_configuration_import", previous);
        } else {
            shortcuts.deleteEntry("line_configuration_import");
        }
        shortcuts.sync();
    });
    fixture.import->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_L));
    collection->writeSettings(nullptr, false, fixture.import);
    QCOMPARE(
        shortcuts.readEntry("line_configuration_import", QString()),
        QStringLiteral("Ctrl+Alt+L")
    );
    desktop_fixture customized;
    QCOMPARE(
        customized.import->shortcut(),
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_L)
    );
#else
    auto* preferences
        = window.findChild<QAction*>(QStringLiteral("main_preferences_action"));
    QVERIFY(preferences != nullptr);
    preferences->trigger();
    QCOMPARE(fixture.preferences_requests, 1);
    QVERIFY(help->actions().contains(
        window.findChild<QAction*>(QStringLiteral("main_about_action"))
    ));
#endif

    // Retain the object names and Qt state tag used by both desktop profiles.
    window.addToolBar(Qt::LeftToolBarArea, toolbar);
    fixture.docks[2]->hide();
    const auto layout = window.saveState(1);
    QTemporaryDir state_directory;
    QVERIFY(state_directory.isValid());
    QSettings saved_state(
        state_directory.filePath(QStringLiteral("window.ini")),
        QSettings::IniFormat
    );
    QVERIFY(yodau::shell::save_main_window_state(window, saved_state));
    QCOMPARE(
        saved_state.value(QStringLiteral("desktop/main_window/layout"))
            .toByteArray(),
        layout
    );
    QVERIFY(
        saved_state.contains(QStringLiteral("desktop/main_window/geometry"))
    );
    desktop_fixture restored;
    QVERIFY(
        yodau::shell::restore_main_window_state(restored.window, saved_state)
    );
    auto* restored_toolbar
        = restored.window.findChild<QToolBar*>(QStringLiteral("main_toolbar"));
    QVERIFY(restored_toolbar != nullptr);
    QCOMPARE(
        restored.window.toolBarArea(restored_toolbar), Qt::LeftToolBarArea
    );
    QVERIFY(restored.docks[2]->isHidden());
    const auto geometry = window.saveGeometry();
    QVERIFY(restored.window.restoreGeometry(geometry));

    bool about_opened = false;
    QTimer::singleShot(0, [&] {
        for (auto* widget : QApplication::topLevelWidgets()) {
            if (auto* dialog = qobject_cast<QDialog*>(widget)) {
                if (dialog->parentWidget() == &window && dialog->isVisible()) {
                    about_opened = true;
                    dialog->reject();
                }
            }
        }
    });
#ifdef KC_KDE
    collection->action(QStringLiteral("help_about_app"))->trigger();
#else
    window.findChild<QAction*>(QStringLiteral("main_about_action"))->trigger();
#endif
    QTRY_VERIFY(about_opened);
#ifdef KC_KDE
    collection->action(QStringLiteral("file_quit"))->trigger();
#else
    window.findChild<QAction*>(QStringLiteral("main_quit_action"))->trigger();
#endif
    QVERIFY(window.isHidden());
#endif
}
