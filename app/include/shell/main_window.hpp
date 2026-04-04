#ifndef YODAU_APP_SHELL_MAIN_WINDOW_HPP
#define YODAU_APP_SHELL_MAIN_WINDOW_HPP

#include "core/namespace_alias.hpp"

#ifdef KC_KDE
#include <KXmlGuiWindow>
using base_main_window = KXmlGuiWindow;
#else
#include <QMainWindow>
using base_main_window = QMainWindow;
#endif

#include <QString>
#include <memory>

namespace yodau::core {
class stream_manager;
}

namespace yodau::monitor {
class runtime_bridge;
}

class stream_board;
class stream_controller;
class settings_panel;
class QAction;
class QDockWidget;
class QLabel;
class QStackedWidget;

class main_window final : public base_main_window {
    Q_OBJECT
public:
    explicit main_window(
        bool enable_debug_monitor = false,
        const QString& debug_monitor_endpoint_name = QString(),
        QWidget* parent = nullptr
    );
    ~main_window() override;

private slots:
    void on_toggle_debug_monitor_triggered(bool checked);
    void refresh_debug_monitor_ui();

private:
    void setup_platform_layout();
    void setup_debug_monitor_ui();

    stream_board* main_zone;
    settings_panel* settings;
    std::unique_ptr<yodau::core::stream_manager> core_manager;
    stream_controller* app_stream_controller;
    yodau::monitor::runtime_bridge* debug_monitor;
    QAction* toggle_debug_monitor_action;
    QLabel* debug_monitor_status_label;

    QStackedWidget* zones_stack { nullptr };
    QDockWidget* settings_dock { nullptr };
    QDockWidget* line_dock { nullptr };
    QDockWidget* log_dock { nullptr };
};

#endif // YODAU_APP_SHELL_MAIN_WINDOW_HPP
