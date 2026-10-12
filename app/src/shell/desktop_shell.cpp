#include "shell/desktop_shell.hpp"
#include "shell/str_label.hpp"

#include <QAction>
#include <QCoreApplication>
#include <QDockWidget>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QToolBar>

#ifdef KC_KDE
#include <KActionCollection>
#include <KCommandBar>
#include <KHelpMenu>
#include <KShortcutsDialog>
#include <KStandardAction>
#include <KToggleAction>
#include <KToolBar>
#include <QIcon>
#include <QStatusBar>
#endif

namespace {

QString tr(const char* text) {
#ifdef KC_KDE
    return i18n(text);
#else
    return QCoreApplication::translate("main_window", text);
#endif
}

#ifdef KC_KDE
void collect_menu_actions(
    QMenu& menu, QList<KCommandBar::ActionGroup>& groups
) {
    KCommandBar::ActionGroup group { menu.title(), {} };
    for (auto* action : menu.actions()) {
        if (auto* submenu = action->menu()) {
            collect_menu_actions(*submenu, groups);
        } else if (!action->isSeparator()) {
            group.actions.append(action);
        }
    }
    groups.append(group);
}
#endif

} // namespace

namespace yodau::shell {

void setup_desktop_shell(
    QMainWindow& window, const desktop_shell_actions& shell,
    const std::function<void()>& show_settings
) {
    auto* file = window.menuBar()->addMenu(tr("&File"));
    file->setObjectName(QStringLiteral("file"));
    auto* view = window.menuBar()->addMenu(tr("&View"));
    view->setObjectName(QStringLiteral("view"));
    auto* settings = window.menuBar()->addMenu(tr("&Settings"));
    settings->setObjectName(QStringLiteral("settings"));

#ifdef KC_KDE
    auto* collection = new KActionCollection(&window);
    collection->setObjectName(QStringLiteral("desktop_action_collection"));
    collection->addAssociatedWidget(&window);
    collection->addAction(
        QStringLiteral("line_configuration_import"), shell.import_configuration
    );
    collection->addAction(
        QStringLiteral("line_configuration_export"), shell.export_configuration
    );
    KActionCollection::setDefaultShortcut(
        shell.import_configuration,
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I)
    );
    KActionCollection::setDefaultShortcut(
        shell.export_configuration,
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E)
    );
    collection->addAction(
        QStringLiteral("view_presentation"), shell.presentation->menuAction()
    );
    const std::array names { "view_streams_dock", "view_lines_dock",
                             "view_logs_dock", "view_processing_dock" };
    for (std::size_t index = 0; index < shell.docks.size(); ++index) {
        if (auto* dock = shell.docks[index]) {
            collection->addAction(
                QString::fromLatin1(names[index]), dock->toggleViewAction()
            );
        }
    }
    auto* quit = KStandardAction::quit(&window, &QWidget::close, collection);
    auto* preferences
        = KStandardAction::preferences(&window, show_settings, collection);
    auto* toolbar = new KToolBar(QStringLiteral("main_toolbar"), &window);
    toolbar->setWindowTitle(tr("Main"));
    window.addToolBar(toolbar);
#else
    auto* quit = new QAction(tr("E&xit"), &window);
    quit->setObjectName(QStringLiteral("main_quit_action"));
    QObject::connect(quit, &QAction::triggered, &window, &QWidget::close);
    auto* preferences = new QAction(tr("Show stream settings"), &window);
    preferences->setObjectName(QStringLiteral("main_preferences_action"));
    QObject::connect(preferences, &QAction::triggered, &window, show_settings);
    auto* toolbar = window.addToolBar(str_label("top"));
    toolbar->setObjectName(QStringLiteral("main_toolbar"));
#endif

    file->addAction(shell.import_configuration);
    file->addAction(shell.export_configuration);
    file->addSeparator();
    file->addAction(quit);
    view->addMenu(shell.presentation);
    for (std::size_t index = 0; index < shell.docks.size(); ++index) {
        if (auto* dock = shell.docks[index]) {
            view->addAction(dock->toggleViewAction());
            if (index < 3) {
                toolbar->addAction(dock->toggleViewAction());
            }
        }
    }

#ifdef KC_KDE
    auto* status = KStandardAction::showStatusbar(
        window.statusBar(), &QWidget::setVisible, collection
    );
    status->setChecked(!window.statusBar()->isHidden());
    settings->addAction(status);
    auto* toolbar_toggle = toolbar->toggleViewAction();
    collection->addAction(QStringLiteral("main_toolbar"), toolbar_toggle);
    settings->addAction(toolbar_toggle);

    auto* help_provider = new KHelpMenu(&window);
    auto* help = help_provider->menu();
    help->setObjectName(QStringLiteral("help"));
    auto* language = help_provider->action(KHelpMenu::menuSwitchLanguage);
    if (language != nullptr) {
        help->removeAction(language);
        settings->addAction(language);
    }
    auto* shortcuts = KStandardAction::keyBindings(
        &window,
        [&window, collection] {
            KShortcutsDialog::showDialog(
                collection, KShortcutsEditor::LetterShortcutsAllowed, &window
            );
        },
        collection
    );
    settings->addAction(shortcuts);
    settings->addAction(preferences);

    auto* find_action
        = collection->addAction(QStringLiteral("open_kcommand_bar"));
    find_action->setText(tr("Find Action…"));
    find_action->setIcon(QIcon::fromTheme(QStringLiteral("search")));
    KActionCollection::setDefaultShortcut(
        find_action, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_I)
    );
    QObject::connect(find_action, &QAction::triggered, &window, [&window] {
        auto* bar = window.findChild<KCommandBar*>(
            QStringLiteral("desktop_command_bar")
        );
        if (bar == nullptr) {
            bar = new KCommandBar(&window);
            bar->setObjectName(QStringLiteral("desktop_command_bar"));
        }
        QList<KCommandBar::ActionGroup> groups;
        for (auto* action : window.menuBar()->actions()) {
            if (auto* menu = action->menu()) {
                collect_menu_actions(*menu, groups);
            }
        }
        bar->setActions(groups);
        bar->show();
    });
    auto* whats_this = help_provider->action(KHelpMenu::menuWhatsThis);
    const auto position
        = whats_this != nullptr ? help->actions().indexOf(whats_this) + 1 : 0;
    help->insertAction(help->actions().value(position), find_action);
    window.menuBar()->addMenu(help);
    for (const auto id :
         { KHelpMenu::menuHelpContents, KHelpMenu::menuWhatsThis,
           KHelpMenu::menuReportBug, KHelpMenu::menuDonate,
           KHelpMenu::menuAboutApp, KHelpMenu::menuAboutKDE,
           KHelpMenu::menuSwitchLanguage }) {
        if (auto* action = help_provider->action(id)) {
            collection->addAction(action->objectName(), action);
        }
    }
    // Load config-based shortcut overrides; window layout remains in QSettings.
    collection->readSettings();
#else
    settings->addAction(preferences);
    auto* help = window.menuBar()->addMenu(tr("&Help"));
    help->setObjectName(QStringLiteral("help"));
    auto* about = help->addAction(tr("&About yodau"));
    about->setObjectName(QStringLiteral("main_about_action"));
    QObject::connect(about, &QAction::triggered, &window, [&window] {
        QMessageBox::about(
            &window, tr("About yodau"),
            tr("yodau %1\nVideo stream analysis and line configuration.")
                .arg(QCoreApplication::applicationVersion())
        );
    });
#endif
}

} // namespace yodau::shell
