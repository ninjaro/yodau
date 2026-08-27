#ifndef YODAU_APP_SHELL_STREAM_CONTROLLER_HPP
#define YODAU_APP_SHELL_STREAM_CONTROLLER_HPP

#include "core/namespace_alias.hpp"
#include <QColor>
#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QPointF>
#include <QRandomGenerator>
#include <QString>
#include <QThreadPool>
#include <QTimer>

#include <atomic>
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
#include "shell/app_log.hpp"
#include "shell/app_settings.hpp"
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
        yodau::core::stream_manager* mgr, settings_panel* panel,
        stream_board* zone, yodau::monitor::runtime_bridge* monitor = nullptr,
        QObject* parent = nullptr
    );
    ~stream_controller() override;

    void init_from_core();

    [[nodiscard]] QString active_configuration_stream_name() const;
    [[nodiscard]] bool export_line_configuration_to(
        const QString& path, QString* error_message = nullptr
    ) const;
    [[nodiscard]] bool export_line_configuration_data(
        QByteArray* contents, QString* error_message = nullptr
    ) const;
    [[nodiscard]] bool import_line_configuration_from(
        const QString& path, QString* error_message = nullptr
    );
    [[nodiscard]] bool import_line_configuration_data(
        const QByteArray& contents, QString* error_message = nullptr,
        QString* imported_stream_name = nullptr
    );
    void focus_stream(const QString& stream_name);
    void return_to_stream_grid();

public slots:
    // add tab
    void handle_add_file(const QString& path, const QString& name, bool loop);
    void handle_add_local(const QString& source, const QString& name);
    void handle_add_url(const QString& url, const QString& name);
    void handle_detect_local_sources();

    // streams tab / grid
    void handle_show_stream_changed(const QString& name, bool show);

    // core
    void handle_core_event(const QString& text);

    void on_gui_frame(const QString& stream_name, const QImage& image);

signals:
    void monitor_gui_frame_observed(
        const QString& stream_name, qint64 estimated_bytes
    );
    void
    monitor_stream_visibility_changed(const QString& stream_name, bool visible);
    void monitor_core_event_observed(const QString& kind);

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
    void on_active_line_detach_requested(const QString& line_name);
    void on_line_edit_preview_changed(line_edit_request request);
    void on_line_edit_preview_cleared();
    void on_line_edit_save_requested(line_edit_request request);

    void
    on_active_template_add_requested(template_apply_settings settings_value);
    void
    on_active_template_settings_changed(template_apply_settings settings_value);

    void on_active_line_undo_requested();
    void
    on_core_frame_processed(const QString& stream_name, int width, int height);
    void on_core_event_queued(const yodau::core::event& event_value);

private:
    // setup
    void setup_settings_connections();
    void setup_grid_connections();
    void on_grid_stream_closed(const QString& name);

    // grid / active helpers
    void handle_enlarge_requested(const QString& name);
    void handle_back_to_grid();
    void handle_thumb_activate(const QString& name);
    void sync_core_stream_algorithm(
        const QString& stream_name, const QString& algorithm_id,
        const QString& preset_id
    );

    void set_active_stream(const QString& name);
    stream_settings settings_for_stream(const QString& name) const;
    QString algorithm_id_for_stream(const QString& name) const;

    void append_log_entry(app_log_entry entry) const;
    void append_log_entries(const QVector<app_log_entry>& entries) const;
    void append_log(
        app_log_area area, app_log_severity severity, const QString& subsystem,
        const QString& message, const QString& stream_name = QString(),
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
    void note_core_frame_observed(
        const QString& stream_name, int width, int height,
        const QString& processing_summary = QString()
    );
    void sync_runtime_metrics_for_stream(const QString& stream_name);
    void sync_visible_runtime_metrics();
    static int interval_ms_for_fps(int fps);
    static int fps_for_interval_ms(int interval_ms);
    static double update_fps_ema(
        std::chrono::steady_clock::time_point& last_sample, double& ema_fps,
        std::chrono::steady_clock::time_point now
    );
    QSize processing_image_size(
        const QString& stream_name, const QImage& image
    ) const;
    void schedule_gui_frame_worker(const QString& stream_name, int delay_ms);
    void drain_latest_gui_frames(const QString& stream_name);
    void update_monitor_inventory();

    void on_core_event(const yodau::core::event& e);
    void on_core_events(const std::vector<yodau::core::event>& evs);

    static yodau::core::frame frame_from_image(const QImage& image);

private:
    yodau::core::processing_runtime core_runtime;

    // external
    yodau::core::stream_manager* stream_mgr { nullptr };
    yodau::monitor::runtime_bridge* monitor_bridge { nullptr };
    settings_panel* settings { nullptr };
    stream_board* main_zone { nullptr };
    grid_view* grid { nullptr };
    app_log_buffer* log_buffer { nullptr };

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

    yodau::core::fps_capability_profile fps_capability;
    QHash<QString, int> processing_scale_percent_by_stream;
    QHash<QString, QString> virtual_camera_path_by_stream;
    QHash<QString, stream_runtime_metrics> runtime_metrics_by_stream;

    struct stream_rate_tracker {
        std::chrono::steady_clock::time_point last_input_frame {};
        std::chrono::steady_clock::time_point last_core_frame {};
        double input_fps_ema { 0.0 };
        double core_fps_ema { 0.0 };
    };

    QHash<QString, stream_rate_tracker> rate_trackers_by_stream;
    double processing_cost_ema_ms { 0.0 };
    std::chrono::steady_clock::time_point last_fps_policy_refresh {};

    struct pending_gui_frame {
        QImage latest_image;
        QSize processing_size;
        bool worker_scheduled { false };
        quint64 dropped_frames { 0 };
        int minimum_interval_ms { 1 };
        std::chrono::steady_clock::time_point last_scheduled {};
    };

    QHash<QString, pending_gui_frame> pending_gui_frames;
    QMutex pending_gui_frames_mutex;
    QThreadPool processing_pool;
    std::atomic_bool shutting_down { false };
};

#endif // YODAU_APP_SHELL_STREAM_CONTROLLER_HPP
