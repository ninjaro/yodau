#ifndef YODAU_FRONTEND_SHELL_STREAM_CONTROLLER_HPP
#define YODAU_FRONTEND_SHELL_STREAM_CONTROLLER_HPP

#include <QColor>
#include <QMap>
#include <QObject>
#include <QPointF>
#include <QRandomGenerator>
#include <QString>
#include <QTimer>

#include <chrono>
#include <deque>
#include <vector>

#include "analysis/fps_policy.hpp"
#include "analysis/processing_runtime.hpp"
#include "shell/active_edit_actions.hpp"
#include "shell/active_edit_controller.hpp"
#include "shell/active_edit_session.hpp"
#include "shell/active_edit_workflow.hpp"
#include "shell/active_stream_state.hpp"
#include "shell/active_stream_workflow.hpp"
#include "shell/frontend_log.hpp"
#include "shell/frontend_settings.hpp"
#include "shell/processing_feedback_state.hpp"
#include "shell/stream_catalog_state.hpp"
#include "shell/stream_catalog_workflow.hpp"
#include "shell/stream_route_state.hpp"
#include "shell/stream_widget_bridge.hpp"
#include "streams/stream_manager.hpp"

class stream_board;
class grid_view;
class settings_panel;
class stream_cell;

namespace yodau::monitor {
class runtime_bridge;
}

class stream_controller final : public QObject {
    Q_OBJECT

public:
    explicit stream_controller(
        yodau::backend::stream_manager* mgr, settings_panel* panel,
        stream_board* zone, yodau::monitor::runtime_bridge* monitor = nullptr,
        QObject* parent = nullptr
    );

    void init_from_backend();

public slots:
    // add tab
    void handle_add_file(const QString& path, const QString& name, bool loop);
    void handle_add_local(const QString& source, const QString& name);
    void handle_add_url(const QString& url, const QString& name);
    void handle_detect_local_sources();

    // streams tab / grid
    void handle_show_stream_changed(const QString& name, bool show);

    // backend
    void handle_backend_event(const QString& text);

    void on_gui_frame(const QString& stream_name, const QImage& image);

signals:
    void monitor_gui_frame_observed(
        const QString& stream_name, qint64 estimated_bytes
    );
    void
    monitor_stream_visibility_changed(const QString& stream_name, bool visible);
    void monitor_backend_event_observed(const QString& kind);

private slots:
    // stream settings tab
    void on_stream_settings_selection_changed(const QString& name);
    void on_active_stream_settings_changed(stream_settings settings_value);

    // line dock
    void on_active_stream_selected(const QString& name);
    void on_active_edit_mode_changed(bool drawing_new);
    void on_active_line_profile_changed(line_profile profile_value);
    void on_active_line_save_requested(line_profile profile_value);
    void on_active_line_enabled_changed(const QString& line_name, bool enabled);
    void on_active_line_edit_preview_changed(line_edit_request request);
    void on_active_line_edit_preview_cleared();
    void on_active_line_edit_save_requested(line_edit_request request);

    void on_active_template_add_requested(
        template_apply_settings settings_value
    );
    void on_active_template_settings_changed(
        template_apply_settings settings_value
    );

    void on_active_line_undo_requested();
    void on_backend_frame_processed(
        QString stream_name, int width, int height
    );
    void on_backend_event_queued(yodau::backend::event event_value);

private:
    // setup
    void setup_settings_connections();
    void setup_grid_connections();
    void on_grid_stream_closed(const QString& name);

    // grid / active helpers
    void handle_enlarge_requested(const QString& name);
    void handle_back_to_grid();
    void handle_thumb_activate(const QString& name);
    void sync_backend_stream_algorithm(
        const QString& stream_name, const QString& algorithm_id
    );

    void set_active_stream(const QString& name);
    stream_settings settings_for_stream(const QString& name) const;
    QString algorithm_id_for_stream(const QString& name) const;

    void append_log_entry(frontend_log_entry entry) const;
    void append_log_entries(const QVector<frontend_log_entry>& entries) const;
    void append_log(
        frontend_log_area area, frontend_log_severity severity,
        const QString& subsystem, const QString& message,
        const QString& stream_name = QString(),
        const QString& detail = QString(),
        const QString& algorithm_id = QString(),
        const QString& line_name = QString(),
        const QString& event_type = QString(),
        const QColor& line_color = QColor()
    ) const;
    QColor resolved_log_line_color(
        const QString& stream_name, const QString& line_name
    ) const;
    QColor resolved_overlay_line_color(
        const QString& stream_name, const QString& line_name,
        const QColor& fallback_color
    ) const;
    void apply_active_edit_result(
        const active_edit_workflow::transition_result& result
    );
    void apply_active_stream_result(
        const active_stream_workflow::transition_result& result
    );
    void apply_catalog_result(
        const stream_catalog_workflow::transition_result& result
    );

    void refresh_fps_policy(bool force = false);
    int grid_cell_count() const;
    int line_count_for_stream(const QString& stream_name) const;
    double current_device_load_ratio() const;
    void note_processing_cost_sample(double elapsed_ms);
    void note_input_frame_observed(
        const QString& stream_name, int width, int height
    );
    void note_backend_frame_observed(
        const QString& stream_name, int width, int height
    );
    void sync_runtime_metrics_for_stream(const QString& stream_name);
    void sync_visible_runtime_metrics();
    static int interval_ms_for_fps(int fps);
    static int fps_for_interval_ms(int interval_ms);
    static double update_fps_ema(
        std::chrono::steady_clock::time_point& last_sample,
        double& ema_fps, std::chrono::steady_clock::time_point now
    );
    QImage scaled_processing_image(
        const QString& stream_name, const QImage& image
    ) const;
    void update_monitor_inventory();

    void on_backend_event(const yodau::backend::event& e);
    void on_backend_events(const std::vector<yodau::backend::event>& evs);

    yodau::backend::frame frame_from_image(const QImage& image) const;

private:
    yodau::backend::processing_runtime backend_runtime;

    // external
    yodau::backend::stream_manager* stream_mgr { nullptr };
    yodau::monitor::runtime_bridge* monitor_bridge { nullptr };
    settings_panel* settings { nullptr };
    stream_board* main_zone { nullptr };
    grid_view* grid { nullptr };
    frontend_log_buffer* log_buffer { nullptr };

    // active state
    active_edit_session edit_session;
    processing_feedback_state feedback_state;
    stream_catalog_state catalog_state;
    stream_route_state route_state;
    stream_widget_bridge widget_bridge;
    stream_catalog_workflow catalog_workflow;
    active_stream_state active_streams;
    active_stream_workflow stream_workflow;
    active_edit_controller edit_controller;
    active_edit_actions edit_actions;
    active_edit_workflow edit_workflow;

    yodau::backend::fps_capability_profile fps_capability;
    QHash<QString, int> processing_scale_percent_by_stream;
    QHash<QString, stream_runtime_metrics> runtime_metrics_by_stream;
    struct stream_rate_tracker {
        std::chrono::steady_clock::time_point last_input_frame {};
        std::chrono::steady_clock::time_point last_backend_frame {};
        double input_fps_ema { 0.0 };
        double backend_fps_ema { 0.0 };
    };
    QHash<QString, stream_rate_tracker> rate_trackers_by_stream;
    double processing_cost_ema_ms { 0.0 };
    std::chrono::steady_clock::time_point last_fps_policy_refresh {};
};

#endif // YODAU_FRONTEND_SHELL_STREAM_CONTROLLER_HPP
