#ifndef YODAU_FRONTEND_WIDGETS_SETTINGS_PANEL_HPP
#define YODAU_FRONTEND_WIDGETS_SETTINGS_PANEL_HPP

#include "shell/frontend_log.hpp"
#include "shell/frontend_settings.hpp"

#include <QColor>
#include <QSet>
#include <QStringList>
#include <QWidget>

class QTabWidget;
class active_editor_panel;
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

    void set_log_buffer(frontend_log_buffer* buffer);
    void set_log_mode(frontend_log_mode mode);
    frontend_log_mode log_mode() const;
    void append_log(frontend_log_entry entry) const;
    QString compose_current_log_report() const;
    QString compose_current_log_summary() const;
    bool write_current_log_report(const QString& path) const;

    // streams tab
    void add_stream_entry(
        const QString& name, const QString& source, bool checked = false
    ) const;
    void set_stream_checked(const QString& name, bool checked) const;
    void remove_stream_entry(const QString& name) const;
    void clear_stream_entries();

    void append_event(const QString& text) const;

    // add tab
    void set_local_sources(const QStringList& sources) const;
    void clear_add_inputs() const;
    void append_add_log(const QString& text) const;

    // active tab: streams
    void set_active_candidates(const QStringList& names) const;
    void set_active_current(const QString& name) const;
    void set_active_stream_settings(const stream_settings& settings_value);
    stream_settings current_active_stream_settings() const;

    // active tab: templates / lines
    void add_template_candidate(const QString& name) const;
    void set_template_candidates(const QStringList& names) const;
    void set_active_line_profile(const line_profile& profile);
    line_profile current_active_line_profile() const;
    void set_active_template_settings(
        const template_apply_settings& settings_value
    );
    template_apply_settings current_active_template_settings() const;

    void reset_active_line_form();
    void reset_active_template_form();

    void set_active_line_closed(bool closed) const;

    QString active_template_current() const;
    QColor active_template_preview_color() const;

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

    // active tab
    void active_stream_settings_changed(stream_settings settings_value);
    void log_mode_changed(frontend_log_mode mode);

    void active_edit_mode_changed(bool drawing_new);

    void active_line_profile_changed(line_profile profile_value);
    void active_line_save_requested(line_profile profile_value);
    void active_line_undo_requested();

    void active_template_add_requested(
        template_apply_settings settings_value
    );
    void active_template_settings_changed(
        template_apply_settings settings_value
    );

private:
    // ui build
    void build_ui();
    QWidget* build_add_tab();
    QWidget* build_streams_tab();
    QWidget* build_active_tab();
    frontend_log_area current_log_area() const;

private slots:
    void on_copy_logs_clicked();
    void on_copy_summary_clicked();
    void on_save_logs_clicked();

private:
    // common
    QTabWidget* tabs;
    log_toolbar_panel* log_toolbar_widget { nullptr };
    QSet<QString> existing_names;

    // add tab
    QWidget* add_tab { nullptr };
    stream_source_panel* source_panel { nullptr };

    // streams tab
    QWidget* streams_tab;
    stream_inventory_panel* inventory_panel { nullptr };

    // active tab
    QWidget* active_tab { nullptr };
    active_editor_panel* active_editor_panel_widget { nullptr };
};

#endif // YODAU_FRONTEND_WIDGETS_SETTINGS_PANEL_HPP
