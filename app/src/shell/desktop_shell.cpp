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

#include <shared_ui/qt/fonts.hpp>
#include <shared_ui/qt/style_sheet.hpp>

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

// The prototype's faces (Noto Sans, DejaVu Sans Mono) ship as Latin subsets
// for platforms without KDE's defaults; installed copies still win.
QString slate_family(
    const QStringList& wanted, const QFontDatabase::SystemFont fallback
) {
    QStringList paths;
    for (const char* name :
         { "NotoSans-Regular", "NotoSans-Medium", "NotoSans-Bold",
           "DejaVuSansMono", "DejaVuSansMono-Bold" })
        paths.append(QStringLiteral(":/yodau/brand/fonts/%1.ttf")
                         .arg(QLatin1String(name)));
    shared_ui::qt::register_fonts(paths);
    return shared_ui::qt::first_family(wanted, fallback);
}

// Slate material of `docs/prototypes/Yodau UI.dc.html`: Breeze-dark
// surfaces, one blue accent for selection and DejaVu-style monospace labels.
constexpr auto slate_style_sheet = R"(
QMainWindow, QDialog { background: #2a2e32; color: #eff0f1; }
QWidget { font-family: "$ui$"; font-size: 12px; }
QMainWindow::separator { background: #24282b; width: 1px; height: 1px; }
QMainWindow::separator:hover { background: #3daee9; }
QMenuBar { background: #2f3439; color: #dce1de; border-bottom: 1px solid #24282b; padding: 1px 4px; }
QMenuBar::item { padding: 4px 8px; background: transparent; }
QMenuBar::item:selected { background: #3b4045; }
QMenu { background: #31363b; color: #eff0f1; border: 1px solid #4d5359; padding: 4px; }
QMenu::item { padding: 5px 24px 5px 24px; border-radius: 2px; }
QMenu::item:selected { background: #3daee9; color: #12191d; }
QMenu::item:disabled { color: #7f8c8d; }
QMenu::separator { height: 1px; background: #4d5359; margin: 4px 6px; }
QToolBar { background: #2f3439; border: none; border-bottom: 1px solid #24282b; padding: 6px 8px; spacing: 6px; }
QToolBar::handle { image: none; width: 0px; }
QToolBar QToolButton { background: #2f3439; color: #a7abae; border: 1px solid #3b4045; border-radius: 2px; padding: 4px 10px; }
QToolBar QToolButton:hover { border-color: #3daee9; }
QToolBar QToolButton:checked { background: #3b4045; color: #eff0f1; border-color: #4d5359; }
QDockWidget { color: #dce1de; }
QDockWidget::title { background: #31363b; padding: 6px 8px; text-align: left; }
QDockWidget::close-button, QDockWidget::float-button { subcontrol-position: top right; background: transparent; border: none; padding: 0px; }
QDockWidget::float-button { right: 20px; }
QDockWidget > QWidget { background: #31363b; }
QStatusBar { background: #2f3439; color: #a7abae; border-top: 1px solid #24282b; }
QLabel { color: #eff0f1; background: transparent; }
QGroupBox { background: transparent; color: #dce1de; border: 1px solid #4d5359; border-radius: 3px; margin-top: 9px; padding: 12px 8px 8px 8px; }
QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top center; padding: 0px 6px; color: #dce1de; background: #31363b; }
QPushButton { background: #3b4045; color: #eff0f1; border: 1px solid #4d5359; border-radius: 2px; padding: 5px 10px; }
QPushButton:hover { border-color: #3daee9; }
QPushButton:focus { border-color: #74c0fc; }
QPushButton:pressed, QPushButton:checked { background: #3daee9; color: #12191d; border-color: #3daee9; }
QPushButton:disabled { background: #31363b; color: #5d666c; border-color: #3b4045; }
QToolButton { background: transparent; color: #eff0f1; border: 1px solid transparent; border-radius: 2px; padding: 3px; }
QToolButton:hover { border-color: #4d5359; }
QToolButton:checked { background: #3daee9; color: #12191d; }
QLineEdit, QComboBox, QAbstractSpinBox, QPlainTextEdit, QTextEdit { background: #232629; color: #eff0f1; border: 1px solid #4d5359; border-radius: 2px; padding: 4px 8px; selection-background-color: #3daee9; selection-color: #12191d; }
QLineEdit:focus, QComboBox:focus, QAbstractSpinBox:focus { border-color: #3daee9; }
QLineEdit:disabled, QComboBox:disabled, QAbstractSpinBox:disabled { color: #7f8c8d; border-color: #3b4045; }
QComboBox::drop-down { border: none; width: 18px; }
QComboBox QAbstractItemView { background: #31363b; color: #eff0f1; border: 1px solid #4d5359; selection-background-color: #3daee9; selection-color: #12191d; outline: none; }
QPlainTextEdit, QTextEdit, QTextBrowser { background: #232629; border-color: #4d5359; font-family: "$mono$"; font-size: 12px; }
QCheckBox, QRadioButton { color: #eff0f1; background: transparent; spacing: 7px; padding-right: 6px; }
QCheckBox::indicator { width: 12px; height: 12px; border: 1px solid #4d5359; background: #232629; border-radius: 2px; }
QRadioButton::indicator { width: 11px; height: 11px; border: 1px solid #4d5359; background: #232629; border-radius: 6px; }
QCheckBox::indicator:checked { background: #3daee9; border-color: #3daee9; }
QRadioButton::indicator:checked { border-color: #3daee9; background: qradialgradient(cx: 0.5, cy: 0.5, radius: 0.5, fx: 0.5, fy: 0.5, stop: 0 #3daee9, stop: 0.48 #3daee9, stop: 0.52 #232629, stop: 1 #232629); }
QCheckBox::indicator:hover, QRadioButton::indicator:hover { border-color: #3daee9; }
QCheckBox:focus, QRadioButton:focus { color: #74c0fc; }
QTabWidget::pane { border: none; border-top: 1px solid #4d5359; background: transparent; top: -1px; }
QTabBar::tab { background: transparent; color: #a7abae; padding: 6px 16px; border: 1px solid transparent; border-bottom: none; border-top-left-radius: 2px; border-top-right-radius: 2px; }
QTabBar::tab:selected { background: #3b4045; color: #eff0f1; border-color: #4d5359; }
QTabBar::tab:hover { color: #eff0f1; }
QHeaderView::section { background: #31363b; color: #a7abae; border: none; border-bottom: 1px solid #4d5359; padding: 5px 8px; }
QTableCornerButton::section { background: #31363b; border: none; }
QTableView, QListView, QTreeView { background: #232629; alternate-background-color: #2a2e32; color: #eff0f1; gridline-color: #3b4045; border: 1px solid #4d5359; selection-background-color: #2e3a44; selection-color: #eff0f1; outline: none; }
QScrollArea { background: transparent; border: none; }
QDockWidget QScrollArea > QWidget, QDockWidget QScrollArea > QWidget > QWidget { background: #31363b; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 0px; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 0px; }
QScrollBar::handle:vertical, QScrollBar::handle:horizontal { background: #4d5359; border-radius: 2px; min-height: 24px; min-width: 24px; }
QScrollBar::handle:hover { background: #3daee9; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0px; height: 0px; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QSlider::groove:horizontal { height: 4px; background: #4d5359; border-radius: 2px; }
QSlider::sub-page:horizontal { background: #3daee9; border-radius: 2px; }
QSlider::handle:horizontal { background: #eff0f1; border: 2px solid #3daee9; width: 10px; height: 10px; margin: -5px 0px; border-radius: 7px; }
QProgressBar { background: #232629; border: 1px solid #4d5359; border-radius: 2px; color: #eff0f1; text-align: center; }
QProgressBar::chunk { background: #2a9d8f; }
QSplitter::handle { background: #24282b; }
QToolTip { background: #31363b; color: #eff0f1; border: 1px solid #3daee9; padding: 4px; }
QScrollArea#stream_grid_scroll_area > QWidget, QScrollArea#stream_grid_scroll_area > QWidget > QWidget { background: #2a2e32; }
stream_cell { background: #232629; color: #dce1de; border: 1px solid #4d5359; }
)";

} // namespace

namespace yodau::shell {

QString desktop_chrome_style_sheet(const desktop_chrome chrome) {
    if (chrome != desktop_chrome::slate)
        return {};
    static const QString mono = slate_family(
        { QStringLiteral("DejaVu Sans Mono"), QStringLiteral("JetBrains Mono"),
          QStringLiteral("SF Mono"), QStringLiteral("Menlo"),
          QStringLiteral("Cascadia Mono"), QStringLiteral("Consolas"),
          QStringLiteral("Noto Sans Mono") },
        QFontDatabase::FixedFont
    );
    static const QString ui = slate_family(
        { QStringLiteral("Noto Sans"), QStringLiteral("DejaVu Sans") },
        QFontDatabase::GeneralFont
    );
    return shared_ui::qt::apply_style_tokens(
        QString::fromLatin1(slate_style_sheet),
        { { QStringLiteral("mono"), mono }, { QStringLiteral("ui"), ui } }
    );
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
