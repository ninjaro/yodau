#include "shell/desktop_shell.hpp"
#include "shell/str_label.hpp"

#include <QAction>
#include <QCoreApplication>
#include <QDockWidget>
#include <QFontDatabase>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStringList>
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

QString first_installed(const QStringList& wanted, const QString& fallback) {
    const QStringList available = QFontDatabase::families();
    for (const QString& family : wanted)
        if (available.contains(family, Qt::CaseInsensitive))
            return family;
    return fallback;
}

// Slate material of `docs/prototypes/Yodau UI.dc.html`: Breeze-dark
// surfaces, one blue accent for selection and DejaVu-style monospace labels.
constexpr auto slate_style_sheet = R"(
QMainWindow, QDialog { background: #232629; color: #eff0f1; }
QMainWindow::separator { background: #31363b; width: 4px; height: 4px; }
QMainWindow::separator:hover { background: #3daee9; }
QMenuBar { background: #232629; color: #eff0f1; border-bottom: 1px solid #31363b; }
QMenuBar::item { padding: 4px 9px; background: transparent; }
QMenuBar::item:selected { background: #31363b; }
QMenu { background: #26292c; color: #eff0f1; border: 1px solid #4d5359; padding: 4px; }
QMenu::item { padding: 5px 24px 5px 24px; border-radius: 2px; }
QMenu::item:selected { background: #3daee9; color: #12191d; }
QMenu::item:disabled { color: #7f8c8d; }
QMenu::separator { height: 1px; background: #3b4045; margin: 4px 6px; }
QToolBar { background: #232629; border: none; border-bottom: 1px solid #31363b; padding: 4px 8px; spacing: 4px; }
QToolBar QToolButton { background: #3b4045; color: #eff0f1; border: 1px solid #4d5359; border-radius: 2px; padding: 4px 10px; }
QToolBar QToolButton:hover { border-color: #3daee9; }
QToolBar QToolButton:checked { background: #3daee9; color: #12191d; border-color: #3daee9; }
QDockWidget { color: #a7abae; }
QDockWidget::title { background: #2a2e32; padding: 6px 8px; border-bottom: 1px solid #31363b; text-align: left; }
QDockWidget > QWidget { background: #232629; }
QStatusBar { background: #232629; color: #a7abae; border-top: 1px solid #31363b; }
QLabel { color: #eff0f1; background: transparent; }
QGroupBox { background: #2a2e32; color: #eff0f1; border: 1px solid #4d5359; border-radius: 2px; margin-top: 16px; padding-top: 4px; }
QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 8px; padding: 0px 2px; color: #7f8c8d; font-family: "$mono$"; font-size: 11px; }
QPushButton { background: #3b4045; color: #eff0f1; border: 1px solid #4d5359; border-radius: 2px; padding: 5px 10px; }
QPushButton:hover { border-color: #3daee9; }
QPushButton:focus { border-color: #74c0fc; }
QPushButton:pressed, QPushButton:checked { background: #3daee9; color: #12191d; border-color: #3daee9; }
QPushButton:disabled { background: #2a2e32; color: #5d666c; border-color: #3b4045; }
QToolButton { background: transparent; color: #eff0f1; border: 1px solid transparent; border-radius: 2px; padding: 3px; }
QToolButton:hover { border-color: #4d5359; }
QToolButton:checked { background: #3daee9; color: #12191d; }
QLineEdit, QComboBox, QAbstractSpinBox, QPlainTextEdit, QTextEdit { background: #232629; color: #eff0f1; border: 1px solid #4d5359; border-radius: 2px; padding: 4px 6px; selection-background-color: #3daee9; selection-color: #12191d; }
QLineEdit:focus, QComboBox:focus, QAbstractSpinBox:focus { border-color: #3daee9; }
QLineEdit:disabled, QComboBox:disabled, QAbstractSpinBox:disabled { color: #7f8c8d; border-color: #3b4045; }
QComboBox QAbstractItemView { background: #26292c; color: #eff0f1; border: 1px solid #4d5359; selection-background-color: #3daee9; selection-color: #12191d; outline: none; }
QPlainTextEdit, QTextEdit, QTextBrowser { background: #12191d; border-color: #3b4045; font-family: "$mono$"; }
QCheckBox, QRadioButton { color: #eff0f1; background: transparent; padding-right: 6px; }
QCheckBox::indicator, QRadioButton::indicator { width: 13px; height: 13px; border: 1px solid #4d5359; background: #232629; }
QCheckBox::indicator { border-radius: 2px; }
QRadioButton::indicator { border-radius: 7px; }
QCheckBox::indicator:checked, QRadioButton::indicator:checked { background: #3daee9; border-color: #3daee9; }
QCheckBox::indicator:hover, QRadioButton::indicator:hover { border-color: #3daee9; }
QCheckBox:focus, QRadioButton:focus { color: #74c0fc; }
QTabWidget::pane { border: 1px solid #3b4045; background: #232629; top: -1px; }
QTabBar::tab { background: transparent; color: #a7abae; padding: 6px 14px; border: none; border-top: 2px solid transparent; }
QTabBar::tab:selected { background: #2a2e32; color: #eff0f1; border-top: 2px solid #3daee9; }
QTabBar::tab:hover { color: #eff0f1; }
QHeaderView::section { background: #2a2e32; color: #7f8c8d; border: none; border-bottom: 1px solid #3b4045; border-right: 1px solid #31363b; padding: 4px 6px; font-family: "$mono$"; font-size: 11px; }
QTableCornerButton::section { background: #2a2e32; border: none; }
QTableView, QListView, QTreeView { background: #232629; alternate-background-color: #26292c; color: #eff0f1; gridline-color: #31363b; border: 1px solid #3b4045; selection-background-color: #2e3a44; selection-color: #eff0f1; outline: none; }
QScrollArea { background: transparent; border: none; }
QDockWidget QScrollArea > QWidget, QDockWidget QScrollArea > QWidget > QWidget { background: #232629; }
QScrollBar:vertical { background: #232629; width: 10px; margin: 0px; }
QScrollBar:horizontal { background: #232629; height: 10px; margin: 0px; }
QScrollBar::handle:vertical, QScrollBar::handle:horizontal { background: #4d5359; border-radius: 2px; min-height: 24px; min-width: 24px; }
QScrollBar::handle:hover { background: #3daee9; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0px; height: 0px; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QSlider::groove:horizontal { height: 4px; background: #4d5359; border-radius: 2px; }
QSlider::sub-page:horizontal { background: #3daee9; border-radius: 2px; }
QSlider::handle:horizontal { background: #eff0f1; border: 2px solid #3daee9; width: 10px; height: 10px; margin: -5px 0px; border-radius: 7px; }
QProgressBar { background: #232629; border: 1px solid #3b4045; border-radius: 2px; color: #eff0f1; text-align: center; }
QProgressBar::chunk { background: #2a9d8f; }
QSplitter::handle { background: #31363b; }
QToolTip { background: #26292c; color: #eff0f1; border: 1px solid #3daee9; padding: 4px; }
QScrollArea#stream_grid_scroll_area > QWidget, QScrollArea#stream_grid_scroll_area > QWidget > QWidget { background: #1b1e20; }
stream_cell { background: #12191d; color: #dce1de; }
)";

} // namespace

namespace yodau::shell {

QString desktop_chrome_style_sheet(const desktop_chrome chrome) {
    if (chrome != desktop_chrome::slate)
        return {};
    static const QString mono = first_installed(
        { QStringLiteral("DejaVu Sans Mono"), QStringLiteral("JetBrains Mono"),
          QStringLiteral("SF Mono"), QStringLiteral("Menlo"),
          QStringLiteral("Cascadia Mono"), QStringLiteral("Consolas"),
          QStringLiteral("Noto Sans Mono") },
        QFontDatabase::systemFont(QFontDatabase::FixedFont).family()
    );
    return QString::fromLatin1(slate_style_sheet)
        .replace(QStringLiteral("$mono$"), mono);
}

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
