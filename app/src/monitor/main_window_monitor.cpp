#include "shell/main_window.hpp"

#if YODAU_DEBUG_OBSERVABILITY

#include "monitor/runtime_bridge.hpp"

#include <QAction>
#include <QLabel>
#include <QString>

void main_window::on_toggle_debug_monitor_triggered(bool checked) {
    if (debug_monitor == nullptr) {
        return;
    }

    debug_monitor->set_enabled(checked);
    refresh_debug_monitor_ui();
}

void main_window::refresh_debug_monitor_ui() {
    const bool enabled
        = debug_monitor != nullptr && debug_monitor->is_enabled();
    if (toggle_debug_monitor_action != nullptr) {
        toggle_debug_monitor_action->setChecked(enabled);
    }

    if (debug_monitor_status_label == nullptr) {
        return;
    }

    const QString endpoint
        = debug_monitor != nullptr ? debug_monitor->endpoint_path() : QString();
    debug_monitor_status_label->setText(
        enabled
            ? QStringLiteral("Monitor: %1")
                  .arg(
                      endpoint.isEmpty() ? QStringLiteral("enabled") : endpoint
                  )
            : QStringLiteral("Monitor: off")
    );
    debug_monitor_status_label->setToolTip(QStringLiteral(
        "Standalone monitor IPC endpoint shown here when debug broadcast is "
        "enabled."
    ));
}

#endif // YODAU_DEBUG_OBSERVABILITY
