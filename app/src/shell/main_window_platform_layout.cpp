#include "shell/main_window.hpp"

#include "shell/str_label.hpp"
#include "widgets/log_area_view.hpp"
#include "widgets/log_toolbar_panel.hpp"
#include "widgets/stream_board.hpp"
#include "widgets/settings_panel.hpp"

#include <QDockWidget>
#include <QStackedWidget>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

void main_window::setup_platform_layout() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    zones_stack = new QStackedWidget(this);
    zones_stack->addWidget(main_zone);
    zones_stack->addWidget(settings);
    zones_stack->setCurrentWidget(main_zone);
    setCentralWidget(zones_stack);
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
    log_dock_content->setObjectName(QStringLiteral("main_window_log_dock_content"));
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

    auto* top_toolbar = addToolBar(str_label("top"));
    top_toolbar->addAction(settings_action);
    top_toolbar->addAction(line_action);
    top_toolbar->addAction(log_action);
#endif
}
