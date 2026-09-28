#ifndef YODAU_APP_SHELL_WINDOW_STATE_STORE_HPP
#define YODAU_APP_SHELL_WINDOW_STATE_STORE_HPP

#include <QString>
#include <array>
#include <optional>

class QDockWidget;
class QMainWindow;
class QSettings;

namespace yodau::shell {

enum class desktop_preset { classic, canvas };
enum class desktop_panel { streams, editor, logs };

struct desktop_presentation {
    desktop_preset preset = desktop_preset::classic;
    // Stable order: streams, editor, logs. Missing values inherit the preset.
    std::array<std::optional<bool>, 3> panel_overrides {};
    [[nodiscard]] bool visible(desktop_panel panel) const;
    void reset_overrides();
    bool operator==(const desktop_presentation&) const = default;
};

// Missing data means keep the user's existing Qt dock state, not reset Classic.
[[nodiscard]] std::optional<desktop_presentation>
load_desktop_presentation(QSettings& settings);
[[nodiscard]] bool save_desktop_presentation(
    QSettings& settings, const desktop_presentation& value
);
void apply_desktop_presentation(
    const desktop_presentation& value, const std::array<QDockWidget*, 3>& docks
);
void capture_desktop_panel_overrides(
    desktop_presentation& value, const std::array<QDockWidget*, 3>& docks
);

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
