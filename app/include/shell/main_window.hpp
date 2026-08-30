#ifndef YODAU_APP_SHELL_MAIN_WINDOW_HPP
#define YODAU_APP_SHELL_MAIN_WINDOW_HPP

#include "core/namespace_alias.hpp"
#include "debug/build_config.hpp"

#ifdef KC_KDE
#include <KXmlGuiWindow>
using base_main_window = KXmlGuiWindow;
#else
#include <QMainWindow>
using base_main_window = QMainWindow;
#endif

#include <QString>
#include <Qt>
#include <memory>

namespace yodau::core {
class stream_manager;
}

#if YODAU_DEBUG_OBSERVABILITY
namespace yodau::monitor {
class runtime_bridge;
}
#endif

class stream_board;
class stream_controller;
class settings_panel;
class QAction;
class QActionGroup;
class QDockWidget;
class QKeyEvent;
class QLabel;
class QStackedWidget;
class QToolBar;

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
    void on_import_line_configuration_triggered();
    void on_export_line_configuration_triggered();
    void show_settings_panel();
#if YODAU_DEBUG_OBSERVABILITY
    void on_toggle_debug_monitor_triggered(bool checked);
    void refresh_debug_monitor_ui();
#endif
    void on_application_state_changed(Qt::ApplicationState state);

private:
    void setup_platform_layout();
    void setup_configuration_actions();
    void setup_desktop_shell();
#if YODAU_DEBUG_OBSERVABILITY
    void setup_debug_monitor_ui();
#endif
    void show_mobile_page(int page_index);
    void restore_mobile_session();
    void persist_mobile_session();
    void import_mobile_configuration(
        const QByteArray& contents, bool activate_stream
    );

protected:
    void keyPressEvent(QKeyEvent* event) override;

    stream_board* main_zone;
    settings_panel* settings;
    std::unique_ptr<yodau::core::stream_manager> core_manager;
    stream_controller* app_stream_controller;
    QAction* import_line_configuration_action;
    QAction* export_line_configuration_action;
#if YODAU_DEBUG_OBSERVABILITY
    yodau::monitor::runtime_bridge* debug_monitor;
    QAction* toggle_debug_monitor_action;
    QLabel* debug_monitor_status_label;
#endif

    QStackedWidget* zones_stack { nullptr };
    QDockWidget* settings_dock { nullptr };
    QDockWidget* line_dock { nullptr };
    QDockWidget* log_dock { nullptr };
    QToolBar* mobile_document_toolbar { nullptr };
    QToolBar* mobile_navigation_toolbar { nullptr };
    QActionGroup* mobile_navigation_actions { nullptr };
    QLabel* mobile_status_label { nullptr };
};

#endif // YODAU_APP_SHELL_MAIN_WINDOW_HPP
