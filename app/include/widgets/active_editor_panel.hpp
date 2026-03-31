#ifndef YODAU_FRONTEND_WIDGETS_ACTIVE_EDITOR_PANEL_HPP
#define YODAU_FRONTEND_WIDGETS_ACTIVE_EDITOR_PANEL_HPP

#include "shell/frontend_log.hpp"
#include "shell/frontend_settings.hpp"

#include <QColor>
#include <QWidget>

class active_stream_panel;
class line_profile_panel;
class log_area_view;
class log_toolbar_panel;
class template_apply_panel;

class active_editor_panel final : public QWidget {
    Q_OBJECT

public:
    explicit active_editor_panel(QWidget* parent = nullptr);

    void set_log_toolbar(log_toolbar_panel* toolbar);

    void set_active_candidates(const QStringList& names) const;
    void set_active_current(const QString& name) const;
    void set_stream_settings(const stream_settings& settings_value);
    stream_settings current_stream_settings() const;

    void add_template_candidate(const QString& name) const;
    void set_template_candidates(const QStringList& names) const;
    void set_line_profile(const line_profile& profile);
    line_profile current_line_profile() const;
    void set_template_settings(const template_apply_settings& settings_value);
    template_apply_settings current_template_settings() const;

    void reset_line_form();
    void reset_template_form();

    void set_line_closed(bool closed) const;

    QString current_template_name() const;
    QColor preview_color() const;

    bool append_log_entry(const frontend_log_entry& entry) const;
    void clear_log() const;

signals:
    void stream_settings_changed(stream_settings settings_value);
    void edit_mode_changed(bool drawing_new);
    void line_profile_changed(line_profile profile_value);
    void line_save_requested(line_profile profile_value);
    void line_undo_requested();
    void template_add_requested(template_apply_settings settings_value);
    void template_settings_changed(template_apply_settings settings_value);

private:
    void build_ui();
    void update_tools() const;

    active_stream_panel* active_stream_panel_widget { nullptr };
    line_profile_panel* active_line_panel { nullptr };
    template_apply_panel* active_template_panel { nullptr };
    log_area_view* active_log_view { nullptr };
};

#endif // YODAU_FRONTEND_WIDGETS_ACTIVE_EDITOR_PANEL_HPP
