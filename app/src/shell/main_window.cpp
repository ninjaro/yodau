#include "shell/main_window.hpp"

#include "shell/stream_controller.hpp"
#include "shell/str_label.hpp"
#include "monitor/runtime_bridge.hpp"
#include "streams/stream_manager.hpp"
#include "widgets/stream_board.hpp"
#include "widgets/settings_panel.hpp"

main_window::main_window(
    bool enable_debug_monitor, const QString& debug_monitor_endpoint_name,
    QWidget* parent
)
    : base_main_window(parent)
    , main_zone(new stream_board(this))
    , settings(new settings_panel(this))
    , app_stream_controller(nullptr)
    , debug_monitor(nullptr)
    , toggle_debug_monitor_action(nullptr)
    , debug_monitor_status_label(nullptr) {
    setup_platform_layout();
    backend_manager = std::make_unique<yodau::backend::stream_manager>();
    auto* mgr = backend_manager.get();

    debug_monitor = new yodau::monitor::runtime_bridge(
        yodau::monitor::runtime_bridge::runtime_options {
            .enabled = enable_debug_monitor,
            .requested_endpoint = debug_monitor_endpoint_name,
        },
        this
    );

    auto* ctrl = new stream_controller(mgr, settings, main_zone, debug_monitor, this);
    app_stream_controller = ctrl;

    connect(
        settings, &settings_panel::add_file_stream, ctrl,
        &stream_controller::handle_add_file
    );
    connect(
        settings, &settings_panel::add_local_stream, ctrl,
        &stream_controller::handle_add_local
    );
    connect(
        settings, &settings_panel::add_url_stream, ctrl,
        &stream_controller::handle_add_url
    );
    connect(
        settings, &settings_panel::detect_local_sources_requested, ctrl,
        &stream_controller::handle_detect_local_sources
    );
    connect(
        settings, &settings_panel::show_stream_changed, ctrl,
        &stream_controller::handle_show_stream_changed
    );
    ctrl->handle_detect_local_sources();
    setup_debug_monitor_ui();
}

main_window::~main_window() = default;
