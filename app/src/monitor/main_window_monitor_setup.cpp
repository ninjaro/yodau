#include "shell/main_window.hpp"

#include "monitor/runtime_bridge.hpp"
#include "shell/str_label.hpp"

#include <QAction>
#include <QLabel>
#include <QStatusBar>
#include <QToolBar>
#include <QtGlobal>

void main_window::setup_debug_monitor_ui() {
    QObject::connect(
        debug_monitor, &yodau::monitor::runtime_bridge::state_changed, this,
        &main_window::refresh_debug_monitor_ui
    );

#if !defined(KC_ANDROID) && !defined(Q_OS_ANDROID)
    auto* debug_toolbar = addToolBar(str_label("debug"));
    toggle_debug_monitor_action
        = debug_toolbar->addAction(str_label("monitor"));
    toggle_debug_monitor_action->setCheckable(true);
    toggle_debug_monitor_action->setToolTip(
        str_label("Enable debug-only standalone monitor broadcasting.")
    );
    QObject::connect(
        toggle_debug_monitor_action, &QAction::triggered, this,
        &main_window::on_toggle_debug_monitor_triggered
    );
#endif

    debug_monitor_status_label = new QLabel(this);
    statusBar()->addPermanentWidget(debug_monitor_status_label);

    refresh_debug_monitor_ui();
}
