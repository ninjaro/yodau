#ifndef YODAU_APP_SHELL_DESKTOP_SHELL_HPP
#define YODAU_APP_SHELL_DESKTOP_SHELL_HPP

#include "shell/window_state_store.hpp"

#include <QString>
#include <array>
#include <functional>

class QAction;
class QDockWidget;
class QMainWindow;
class QMenu;

namespace yodau::shell {

struct desktop_shell_actions {
    QAction* import_configuration;
    QAction* export_configuration;
    QMenu* presentation;
    // Streams, Lines, Logs, Processing; the first three also appear on the
    // toolbar.
    std::array<QDockWidget*, 4> docks;
};

void setup_desktop_shell(
    QMainWindow& window, const desktop_shell_actions& actions,
    const std::function<void()>& show_settings
);

// Window-wide style sheet for a chrome choice; empty for the native style.

} // namespace yodau::shell

#endif // YODAU_APP_SHELL_DESKTOP_SHELL_HPP
