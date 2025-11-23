#ifndef YODAU_FRONTEND_HELPERS_CONTROLLER_HPP
#define YODAU_FRONTEND_HELPERS_CONTROLLER_HPP

#include <QColor>
#include <QMap>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QTimer>

#include <vector>

#include "stream_manager.hpp"
#include "widgets/stream_cell.hpp"

class board;
class grid_view;
class settings_panel;

class controller final : public QObject {
    Q_OBJECT

public:
    explicit controller(
        yodau::backend::stream_manager* mgr, settings_panel* panel, board* zone,
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

private:
    // setup
    void setup_settings_connections();
    void setup_grid_connections();

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

    void update_repaint_caps();

    std::vector<yodau::backend::event> make_fake_events(
        const yodau::backend::stream& s, const yodau::backend::frame& f
    ) const;

    void on_backend_event(const yodau::backend::event& e);

private:
    // external
    yodau::backend::stream_manager* stream_mgr { nullptr };
    settings_panel* settings { nullptr };
    board* main_zone { nullptr };
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

    int active_interval_ms { 33 };
    int idle_interval_ms { 66 };
};

#endif // YODAU_FRONTEND_HELPERS_CONTROLLER_HPP
