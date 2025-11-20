#include "main_window.hpp"
#include "helpers/str_label.hpp"
#include "widgets/settings_panel.hpp"
#include "widgets/view_zone.hpp"

#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
#include <QStackedWidget>
#else
#include <QDockWidget>
#endif

main_window::main_window(QWidget* parent)
    : BaseMainWindow(parent)
    , main_zone(new view_zone(this))
    , settings(new settings_panel(this))
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    , zones_stack(new QStackedWidget(this))
#else
    , settings_dock(new QDockWidget(str_label("settings"), this))
#endif
{
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    zones_stack->addWidget(main_zone);
    zones_stack->addWidget(settings);
    zones_stack->setCurrentWidget(main_zone);
    setCentralWidget(zones_stack);
#else
    setCentralWidget(main_zone);

    settings_dock->setWidget(settings);
    settings_dock->setAllowedAreas(
        Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea
    );
    settings_dock->setFeatures(
        QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable
    );
    addDockWidget(Qt::RightDockWidgetArea, settings_dock);
    const auto settings_action = settings_dock->toggleViewAction();
    settings_action->setText(str_label("settings"));

    const auto top_toolbar = addToolBar(str_label("top"));
    top_toolbar->addAction(settings_action);
#endif
}
