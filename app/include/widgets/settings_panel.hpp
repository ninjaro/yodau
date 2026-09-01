#ifndef YODAU_APP_WIDGETS_SETTINGS_PANEL_HPP
#define YODAU_APP_WIDGETS_SETTINGS_PANEL_HPP

#include "shell/app_log.hpp"
#include "shell/app_settings.hpp"
#include "widgets/stream_cell.hpp"

#include <QColor>
#include <QSet>
#include <QStringList>
#include <QWidget>

class QTabWidget;
class QCheckBox;
class QComboBox;
class active_editor_panel;
class active_stream_panel;
class log_toolbar_panel;
class stream_inventory_panel;
class stream_source_panel;

class settings_panel final : public QWidget {
    Q_OBJECT
public:
    explicit settings_panel(QWidget* parent = nullptr);

    // existing names
    void set_existing_names(QSet<QString> names);
    void add_existing_name(const QString& name);
    void remove_existing_name(const QString& name);

    void set_log_buffer(app_log_buffer* buffer);
    void set_log_mode(app_log_mode mode);
    [[nodiscard]] app_log_mode log_mode() const;
    void append_log(app_log_entry entry) const;
    [[nodiscard]] QString compose_current_log_report() const;
    [[nodiscard]] QString compose_current_log_summary() const;
    [[nodiscard]] bool write_current_log_report(const QString& path) const;
    log_toolbar_panel* take_log_toolbar_widget();
    QWidget* take_active_editor_widget();

    // streams tab
    void add_stream_entry(
        const QString& name, const QString& source, bool checked = false
    ) const;
    void set_stream_checked(const QString& name, bool checked) const;
    void remove_stream_entry(const QString& name) const;
    void clear_stream_entries();

    void append_event(const QString& text) const;
    void set_grid_scroll_direction_id(const QString& direction_id);
    [[nodiscard]] QString current_grid_scroll_direction_id() const;
    void set_grid_preserve_order(bool preserve);
    [[nodiscard]] bool grid_preserves_order() const;
    void set_grid_sizing_mode_id(const QString& mode_id);
    [[nodiscard]] QString current_grid_sizing_mode_id() const;

    // add tab
    void set_local_sources(const QStringList& sources) const;
    void set_local_sources(const QList<local_source_descriptor>& sources) const;
    void clear_add_inputs() const;
    void append_add_log(const QString& text) const;

    // active tab: streams
    void set_active_candidates(const QStringList& names) const;
    void set_active_current(const QString& name) const;
    void set_active_stream_settings(const stream_settings& settings_value);
    [[nodiscard]] stream_settings current_active_stream_settings() const;

    // active tab: templates / lines
    void add_template_candidate(const QString& name) const;
    void set_template_candidates(const QStringList& names) const;
    void set_active_line_profile(const line_profile& profile);
    [[nodiscard]] line_profile current_active_line_profile() const;
    void
    set_active_template_settings(const template_apply_settings& settings_value);
    [[nodiscard]] template_apply_settings
    current_active_template_settings() const;

    void reset_active_line_form();
    void reset_active_template_form();

    void set_active_line_closed(bool closed) const;
    void set_active_lines(const std::vector<stream_cell::line_instance>& lines);
    [[nodiscard]] bool select_active_line_edit_point(int visible_index) const;
    [[nodiscard]] bool
    translate_active_line_edit_shape(const QPointF& delta_pct) const;
    [[nodiscard]] bool move_active_line_edit_point(
        int visible_index, const QPointF& point_pct
    ) const;
    [[nodiscard]] bool split_active_line_edit_point(int visible_index) const;
    [[nodiscard]] bool insert_active_line_point_after(
        int visible_segment_index, const QPointF& point_pct
    ) const;
    [[nodiscard]] bool delete_active_line_edit_point(int visible_index) const;
    [[nodiscard]] bool rotate_active_line_edit_shape(
        double delta_degrees, int visible_pivot_index = -1
    ) const;
    void begin_active_line_edit_change() const;
    void finish_active_line_edit_change() const;
    [[nodiscard]] bool undo_active_line_edit_change() const;
    [[nodiscard]] bool redo_active_line_edit_change() const;
    [[nodiscard]] bool revert_active_line_edit_changes() const;

    [[nodiscard]] QString active_template_current() const;
    [[nodiscard]] QColor active_template_preview_color() const;

    void append_active_log(const QString& msg) const;
    void clear_active_log() const;

signals:
    // add tab
    void add_file_stream(const QString& path, const QString& name, bool loop);
    void add_local_stream(const QString& source, const QString& name);
    void add_url_stream(const QString& url, const QString& name);
    void detect_local_sources_requested();

    // streams tab
    void show_stream_changed(const QString& name, bool show);
    void grid_scroll_direction_changed(const QString& direction_id);
    void grid_preserve_order_changed(bool preserve);
    void grid_sizing_mode_changed(const QString& mode_id);

    // stream settings tab
    void stream_settings_selection_changed(const QString& name);
    void active_stream_settings_changed(stream_settings settings_value);
    void log_mode_changed(app_log_mode mode);

    // line dock
    void active_stream_selected(const QString& name);
    void active_edit_mode_changed(bool drawing_new);
    void active_line_profile_changed(line_profile profile_value);
    void active_line_save_requested(line_profile profile_value);
    void active_line_undo_requested();
    void active_line_enabled_changed(const QString& line_name, bool enabled);
    void active_line_detach_requested(const QString& line_name);
    void active_line_edit_preview_changed(line_edit_request request);
    void active_line_edit_preview_cleared();
    void active_line_edit_save_requested(line_edit_request request);

    void active_template_add_requested(template_apply_settings settings_value);
    void
    active_template_settings_changed(template_apply_settings settings_value);

private:
    // ui build
    void build_ui();
    QWidget* build_streams_tab();
    QWidget* build_stream_settings_tab();
    [[nodiscard]] QStringList configured_stream_names() const;
    void sync_stream_settings_candidates() const;

private slots:
    void on_copy_logs_clicked() const;
    void on_copy_summary_clicked() const;
    void on_save_logs_clicked();

private:
    // common
    QTabWidget* tabs;
    log_toolbar_panel* log_toolbar_widget { nullptr };
    QSet<QString> existing_names;

    // streams tab
    stream_source_panel* source_panel { nullptr };
    QWidget* streams_tab;
    stream_inventory_panel* inventory_panel { nullptr };
    QComboBox* grid_scroll_direction_combo_ { nullptr };
    QCheckBox* grid_preserve_order_checkbox_ { nullptr };
    QComboBox* grid_sizing_mode_combo_ { nullptr };

    // stream settings tab
    QWidget* stream_settings_tab { nullptr };
    active_stream_panel* stream_settings_panel_widget { nullptr };

    // line dock
    active_editor_panel* active_editor_panel_widget { nullptr };
};

#endif // YODAU_APP_WIDGETS_SETTINGS_PANEL_HPP
