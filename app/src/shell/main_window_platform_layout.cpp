#include "shell/main_window.hpp"

#include "shell/mobile_session_store.hpp"
#include "shell/str_label.hpp"
#include "widgets/log_area_view.hpp"
#include "widgets/log_toolbar_panel.hpp"
#include "widgets/settings_panel.hpp"
#include "widgets/stream_board.hpp"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCoreApplication>
#include <QDockWidget>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QStyle>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#ifdef KC_KDE
#include <KActionCollection>
#include <KStandardAction>
#endif

void main_window::setup_platform_layout() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    auto* mobile_shell = new QWidget(this);
    mobile_shell->setObjectName(QStringLiteral("mobile_shell"));
    auto* mobile_layout = new QVBoxLayout(mobile_shell);
    mobile_layout->setContentsMargins(0, 0, 0, 0);
    mobile_layout->setSpacing(0);

    mobile_document_toolbar = new QToolBar(tr("Documents"), mobile_shell);
    mobile_document_toolbar->setObjectName(
        QStringLiteral("mobile_document_toolbar")
    );
    mobile_document_toolbar->setMovable(false);
    mobile_document_toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    mobile_document_toolbar->setMinimumHeight(52);
    mobile_layout->addWidget(mobile_document_toolbar);

    mobile_status_label = new QLabel(mobile_shell);
    mobile_status_label->setObjectName(QStringLiteral("mobile_status_label"));
    mobile_status_label->setWordWrap(true);
    mobile_status_label->setMargin(8);
    mobile_status_label->setAccessibleName(tr("Session status"));
    mobile_status_label->hide();
    mobile_layout->addWidget(mobile_status_label);

    zones_stack = new QStackedWidget(mobile_shell);
    zones_stack->setObjectName(QStringLiteral("mobile_page_stack"));
    zones_stack->addWidget(main_zone);

    auto* streams_scroll = new QScrollArea(zones_stack);
    streams_scroll->setObjectName(QStringLiteral("mobile_streams_scroll"));
    streams_scroll->setWidgetResizable(true);
    streams_scroll->setFrameShape(QFrame::NoFrame);
    streams_scroll->setWidget(settings);
    zones_stack->addWidget(streams_scroll);

    if (auto* line_editor = settings->take_active_editor_widget()) {
        line_editor->setObjectName(QStringLiteral("mobile_line_editor_page"));
        auto* lines_scroll = new QScrollArea(zones_stack);
        lines_scroll->setObjectName(QStringLiteral("mobile_lines_scroll"));
        lines_scroll->setWidgetResizable(true);
        lines_scroll->setFrameShape(QFrame::NoFrame);
        lines_scroll->setWidget(line_editor);
        zones_stack->addWidget(lines_scroll);
    } else {
        zones_stack->addWidget(new QWidget(zones_stack));
    }

    auto* log_page = new QWidget(zones_stack);
    log_page->setObjectName(QStringLiteral("mobile_log_page"));
    auto* log_layout = new QVBoxLayout(log_page);
    log_layout->setContentsMargins(8, 8, 8, 8);
    if (auto* log_toolbar = settings->take_log_toolbar_widget()) {
        log_toolbar->setParent(log_page);
        log_layout->addWidget(log_toolbar);
        auto* log_view = new log_area_view(std::nullopt, log_page);
        log_view->setObjectName(QStringLiteral("mobile_log_view"));
        log_view->set_log_toolbar(log_toolbar);
        log_layout->addWidget(log_view, 1);
    }
    zones_stack->addWidget(log_page);
    zones_stack->setCurrentWidget(main_zone);
    mobile_layout->addWidget(zones_stack, 1);

    mobile_navigation_toolbar = new QToolBar(tr("Navigation"), mobile_shell);
    mobile_navigation_toolbar->setObjectName(
        QStringLiteral("mobile_navigation_toolbar")
    );
    mobile_navigation_toolbar->setMovable(false);
    mobile_navigation_toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    mobile_navigation_toolbar->setMinimumHeight(68);
    mobile_navigation_actions = new QActionGroup(this);
    mobile_navigation_actions->setExclusive(true);

    const auto add_mobile_page_action = [this](
                                            const QString& text,
                                            const QIcon& icon,
                                            const yodau::shell::mobile_page page
                                        ) {
        auto* action = mobile_navigation_toolbar->addAction(icon, text);
        action->setCheckable(true);
        action->setData(static_cast<int>(page));
        action->setObjectName(
            QStringLiteral("mobile_navigation_%1").arg(static_cast<int>(page))
        );
        mobile_navigation_actions->addAction(action);
        return action;
    };
    add_mobile_page_action(
        tr("Monitor"), style()->standardIcon(QStyle::SP_ComputerIcon),
        yodau::shell::mobile_page::monitor
    )
        ->setChecked(true);
    add_mobile_page_action(
        tr("Streams"), style()->standardIcon(QStyle::SP_FileDialogListView),
        yodau::shell::mobile_page::streams
    );
    add_mobile_page_action(
        tr("Lines"), style()->standardIcon(QStyle::SP_FileDialogDetailedView),
        yodau::shell::mobile_page::lines
    );
    add_mobile_page_action(
        tr("Logs"), style()->standardIcon(QStyle::SP_FileDialogInfoView),
        yodau::shell::mobile_page::logs
    );
    mobile_layout->addWidget(mobile_navigation_toolbar);

    connect(
        mobile_navigation_actions, &QActionGroup::triggered, this,
        [this](QAction* action) {
            if (action != nullptr) {
                show_mobile_page(action->data().toInt());
            }
        }
    );
    connect(
        zones_stack, &QStackedWidget::currentChanged, this,
        [this](const int index) {
            if (mobile_navigation_actions != nullptr) {
                for (QAction* action : mobile_navigation_actions->actions()) {
                    action->setChecked(action->data().toInt() == index);
                }
            }
            QSettings mobile_settings(
                QStringLiteral("ninjaro"), QStringLiteral("yodau")
            );
            static_cast<void>(yodau::shell::save_mobile_navigation(
                mobile_settings, yodau::shell::normalized_mobile_page(index)
            ));
        }
    );

    mobile_shell->setStyleSheet(QStringLiteral(
        "QPushButton, QToolButton, QComboBox, QSpinBox, QLineEdit {"
        " min-height: 44px; }"
        "QPushButton { padding: 6px 12px; border-radius: 10px; }"
        "QTabBar::tab { min-height: 44px; padding: 6px 12px; }"
        "QAbstractItemView::item { min-height: 40px; }"
        "QCheckBox, QRadioButton { spacing: 10px; padding: 6px 2px; }"
        "QScrollBar:vertical { width: 18px; }"
        "QScrollBar:horizontal { height: 18px; }"
    ));
    setCentralWidget(mobile_shell);
#else
    settings_dock = new QDockWidget(str_label("streams"), this);
    line_dock = new QDockWidget(str_label("lines"), this);
    log_dock = new QDockWidget(str_label("logs"), this);
    setCentralWidget(main_zone);

    settings_dock->setObjectName(QStringLiteral("main_window_streams_dock"));
    settings_dock->setWidget(settings);
    settings_dock->setAllowedAreas(
        Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea
    );
    settings_dock->setFeatures(
        QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable
    );
    addDockWidget(Qt::RightDockWidgetArea, settings_dock);
    auto* settings_action = settings_dock->toggleViewAction();
    settings_action->setText(str_label("streams"));

    if (auto* line_editor = settings->take_active_editor_widget()) {
        line_dock->setObjectName(QStringLiteral("main_window_line_dock"));
        line_dock->setWidget(line_editor);
        line_dock->setAllowedAreas(
            Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea
        );
        line_dock->setFeatures(
            QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable
        );
        addDockWidget(Qt::LeftDockWidgetArea, line_dock);
    }
    auto* line_action = line_dock->toggleViewAction();
    line_action->setText(str_label("lines"));

    auto* log_dock_content = new QWidget(log_dock);
    log_dock_content->setObjectName(
        QStringLiteral("main_window_log_dock_content")
    );
    auto* log_dock_layout = new QVBoxLayout(log_dock_content);
    log_dock_layout->setContentsMargins(8, 8, 8, 8);

    if (auto* log_toolbar = settings->take_log_toolbar_widget()) {
        log_toolbar->setParent(log_dock_content);
        log_dock_layout->addWidget(log_toolbar);

        auto* dock_log_view = new log_area_view(std::nullopt, log_dock_content);
        dock_log_view->setObjectName(QStringLiteral("main_window_log_view"));
        dock_log_view->set_log_toolbar(log_toolbar);
        log_dock_layout->addWidget(dock_log_view, 1);
    }

    log_dock->setObjectName(QStringLiteral("main_window_log_dock"));
    log_dock->setWidget(log_dock_content);
    log_dock->setAllowedAreas(
        Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea
        | Qt::LeftDockWidgetArea
    );
    log_dock->setFeatures(
        QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable
    );
    addDockWidget(Qt::BottomDockWidgetArea, log_dock);
    auto* log_action = log_dock->toggleViewAction();
    log_action->setText(str_label("logs"));

#if !defined(KC_KDE)
    auto* top_toolbar = addToolBar(str_label("top"));
    top_toolbar->setObjectName(QStringLiteral("main_toolbar"));
    top_toolbar->addAction(settings_action);
    top_toolbar->addAction(line_action);
    top_toolbar->addAction(log_action);
#endif
#endif
}

void main_window::show_mobile_page(const int page_index) {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    if (zones_stack != nullptr) {
        zones_stack->setCurrentIndex(
            static_cast<int>(yodau::shell::normalized_mobile_page(page_index))
        );
    }
#else
    Q_UNUSED(page_index);
#endif
}

void main_window::show_settings_panel() {
#if !defined(KC_ANDROID) && !defined(Q_OS_ANDROID)
    if (settings_dock != nullptr) {
        settings_dock->show();
        settings_dock->raise();
        settings_dock->setFocus(Qt::ShortcutFocusReason);
    }
#else
    if (zones_stack != nullptr) {
        show_mobile_page(static_cast<int>(yodau::shell::mobile_page::streams));
    }
#endif
}

void main_window::setup_desktop_shell() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    import_line_configuration_action->setText(tr("Import"));
    export_line_configuration_action->setText(tr("Export"));
    import_line_configuration_action->setShortcut(QKeySequence());
    export_line_configuration_action->setShortcut(QKeySequence());
    import_line_configuration_action->setIcon(
        style()->standardIcon(QStyle::SP_DialogOpenButton)
    );
    export_line_configuration_action->setIcon(
        style()->standardIcon(QStyle::SP_DialogSaveButton)
    );
    if (mobile_document_toolbar != nullptr) {
        mobile_document_toolbar->addAction(import_line_configuration_action);
        mobile_document_toolbar->addAction(export_line_configuration_action);
    }
    return;
#elif defined(KC_KDE)
    auto* actions = actionCollection();
    actions->addAction(
        QStringLiteral("line_configuration_import"),
        import_line_configuration_action
    );
    actions->addAction(
        QStringLiteral("line_configuration_export"),
        export_line_configuration_action
    );
    KActionCollection::setDefaultShortcut(
        import_line_configuration_action,
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I)
    );
    KActionCollection::setDefaultShortcut(
        export_line_configuration_action,
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E)
    );

    auto register_dock_action
        = [actions](QDockWidget* dock, const QString& action_name) {
              if (dock == nullptr) {
                  return;
              }
              auto* action = dock->toggleViewAction();
              action->setObjectName(action_name);
              actions->addAction(action_name, action);
          };
    register_dock_action(settings_dock, QStringLiteral("view_streams_dock"));
    register_dock_action(line_dock, QStringLiteral("view_lines_dock"));
    register_dock_action(log_dock, QStringLiteral("view_logs_dock"));

    if (toggle_debug_monitor_action != nullptr) {
        toggle_debug_monitor_action->setObjectName(
            QStringLiteral("debug_monitor_toggle")
        );
        actions->addAction(
            QStringLiteral("debug_monitor_toggle"), toggle_debug_monitor_action
        );
    }

    KStandardAction::quit(this, &QWidget::close, actions);
    KStandardAction::preferences(
        this, &main_window::show_settings_panel, actions
    );
    setupGUI(
        KXmlGuiWindow::ToolBar | KXmlGuiWindow::Keys | KXmlGuiWindow::StatusBar
            | KXmlGuiWindow::Create,
        QStringLiteral(":/yodau/yodauui.rc")
    );
#else
    auto* file_menu = menuBar()->addMenu(tr("&File"));
    file_menu->addAction(import_line_configuration_action);
    file_menu->addAction(export_line_configuration_action);
    file_menu->addSeparator();
    auto* quit_action = file_menu->addAction(tr("E&xit"));
    quit_action->setObjectName(QStringLiteral("main_quit_action"));
    connect(quit_action, &QAction::triggered, this, &QWidget::close);

    auto* view_menu = menuBar()->addMenu(tr("&View"));
    if (settings_dock != nullptr) {
        view_menu->addAction(settings_dock->toggleViewAction());
    }
    if (line_dock != nullptr) {
        view_menu->addAction(line_dock->toggleViewAction());
    }
    if (log_dock != nullptr) {
        view_menu->addAction(log_dock->toggleViewAction());
    }

    auto* settings_menu = menuBar()->addMenu(tr("&Settings"));
    auto* preferences_action
        = settings_menu->addAction(tr("Show stream settings"));
    preferences_action->setObjectName(
        QStringLiteral("main_preferences_action")
    );
    connect(
        preferences_action, &QAction::triggered, this,
        &main_window::show_settings_panel
    );

    auto* help_menu = menuBar()->addMenu(tr("&Help"));
    auto* about_action = help_menu->addAction(tr("&About yodau"));
    about_action->setObjectName(QStringLiteral("main_about_action"));
    connect(about_action, &QAction::triggered, this, [this]() {
        QMessageBox::about(
            this, tr("About yodau"),
            tr("yodau %1\nVideo stream analysis and line configuration.")
                .arg(QCoreApplication::applicationVersion())
        );
    });
#endif
}
