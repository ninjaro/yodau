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
#include "streams/stream_manager.hpp"
#include "widgets/stream_cell.hpp"

class stream_board;
class grid_view;
class settings_panel;

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
    // active tab
    void on_active_stream_selected(const QString& name);
    void on_active_edit_mode_changed(bool drawing_new);
    void on_active_line_params_changed(
        const QString& name, const QColor& color, bool closed
    );
    void on_active_line_save_requested(const QString& name, bool closed);

    void on_active_template_selected(const QString& template_name);
    void on_active_template_color_changed(const QColor& color);
    void on_active_template_add_requested(
        const QString& template_name, const QColor& color
    );

    void on_active_line_undo_requested();
    void on_active_labels_enabled_changed(bool on);
    void on_backend_event_queued(yodau::backend::event event_value);

private:
    static QString backend_event_kind_text(yodau::backend::event_kind kind);

    // setup
    void setup_settings_connections();
    void setup_grid_connections();
    void on_grid_stream_closed(const QString& name);

    // add helpers
    void handle_add_stream_common(
        const QString& source, const QString& name, const QString& type,
        bool loop
    );
    void register_stream_in_ui(
        const QString& final_name, const QString& source_desc
    );
    static QString now_ts();

    // grid / active helpers
    void handle_enlarge_requested(const QString& name);
    void handle_back_to_grid();
    void handle_thumb_activate(const QString& name);

    stream_cell* active_cell_checked(const QString& fail_prefix);

    void sync_active_persistent();
    void apply_template_preview(const QString& template_name);

    void log_active(const QString& msg) const;

    static QString points_str_from_pct(const std::vector<QPointF>& pts);

    void apply_added_line(
        stream_cell* cell, const QString& final_name,
        const std::vector<QPointF>& pts, bool closed
    );
    void sync_active_cell_lines() const;
    QSet<QString> used_template_names_for_stream(const QString& stream) const;
    QStringList template_candidates_excluding(const QSet<QString>& used) const;

    void refresh_fps_policy(bool force = false);
    int grid_cell_count() const;
    int line_count_for_stream(const QString& stream_name) const;
    int recent_motion_count();
    double current_device_load_ratio() const;
    void note_processing_cost_sample(double elapsed_ms);
    QImage scaled_processing_image(
        const QString& stream_name, const QImage& image
    ) const;
    void update_monitor_inventory();

    void on_backend_event(const yodau::backend::event& e);
    void on_backend_events(const std::vector<yodau::backend::event>& evs);
    stream_cell* tile_for_stream_name(const QString& name) const;

    yodau::backend::frame frame_from_image(const QImage& image) const;

private:
    yodau::backend::processing_runtime backend_runtime;

    // external
    yodau::backend::stream_manager* stream_mgr { nullptr };
    yodau::monitor::runtime_bridge* monitor_bridge { nullptr };
    settings_panel* settings { nullptr };
    stream_board* main_zone { nullptr };
    grid_view* grid { nullptr };

    // active state
    QString active_name;
    bool drawing_new_mode { true };
    bool active_labels_enabled { true };

    QString draft_line_name;
    QColor draft_line_color { Qt::red };
    bool draft_line_closed { false };

    struct tpl_line {
        std::vector<QPointF> pts_pct;
        bool closed { false };
    };

    QMap<QString, tpl_line> templates;
    QMap<QString, std::vector<stream_cell::line_instance>> per_stream_lines;

    QMap<QString, QUrl> stream_sources;
    QMap<QString, bool> stream_loops;

    QHash<QString, QDateTime> last_gui_motion_event_ts;
    int motion_gui_interval_ms { 80 };
    yodau::backend::fps_capability_profile fps_capability;
    QHash<QString, int> processing_scale_percent_by_stream;
    double processing_cost_ema_ms { 0.0 };
    std::deque<std::chrono::steady_clock::time_point> recent_motion_events;
    std::chrono::steady_clock::time_point last_fps_policy_refresh {};
};

#endif // YODAU_FRONTEND_SHELL_STREAM_CONTROLLER_HPP
