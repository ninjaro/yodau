#include "shell/main_window.hpp"

#include "shell/str_label.hpp"
#include "widgets/stream_board.hpp"
#include "widgets/settings_panel.hpp"

#include <QDockWidget>
#include <QStackedWidget>
#include <QToolBar>

void main_window::setup_platform_layout() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    zones_stack = new QStackedWidget(this);
    zones_stack->addWidget(main_zone);
    zones_stack->addWidget(settings);
    zones_stack->setCurrentWidget(main_zone);
    setCentralWidget(zones_stack);
#else
    settings_dock = new QDockWidget(str_label("settings"), this);
    setCentralWidget(main_zone);

    settings_dock->setWidget(settings);
    settings_dock->setAllowedAreas(
        Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea
    );
    settings_dock->setFeatures(
        QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable
    );
    addDockWidget(Qt::RightDockWidgetArea, settings_dock);
    auto* settings_action = settings_dock->toggleViewAction();
    settings_action->setText(str_label("settings"));

    auto* top_toolbar = addToolBar(str_label("top"));
    top_toolbar->addAction(settings_action);
#endif
}
