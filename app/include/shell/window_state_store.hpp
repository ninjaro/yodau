#ifndef YODAU_APP_SHELL_WINDOW_STATE_STORE_HPP
#define YODAU_APP_SHELL_WINDOW_STATE_STORE_HPP

#include <QString>

class QMainWindow;
class QSettings;

namespace yodau::shell {

[[nodiscard]] bool save_main_window_state(
    const QMainWindow& window, QSettings& settings,
    QString* error_message = nullptr
);
[[nodiscard]] bool restore_main_window_state(
    QMainWindow& window, QSettings& settings, QString* error_message = nullptr
);
[[nodiscard]] bool save_main_window_state(
    const QMainWindow& window, QString* error_message = nullptr
);
[[nodiscard]] bool restore_main_window_state(
    QMainWindow& window, QString* error_message = nullptr
);

} // namespace yodau::shell

#endif // YODAU_APP_SHELL_WINDOW_STATE_STORE_HPP
