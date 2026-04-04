#ifndef YODAU_APP_SHELL_STREAM_WIDGET_BRIDGE_HPP
#define YODAU_APP_SHELL_STREAM_WIDGET_BRIDGE_HPP

#include "shell/active_editor_bridge.hpp"
#include "shell/app_log.hpp"
#include "shell/app_settings.hpp"

#include <QString>

class active_edit_session;
class grid_view;
class settings_panel;
class stream_board;
class stream_cell;
class stream_route_state;

class stream_widget_bridge final {
public:
    struct grid_stream_binding {
        QString path;
        QString type;
        bool loop { true };
    };

    stream_widget_bridge(
        stream_board* main_zone = nullptr, settings_panel* settings = nullptr
    );

    grid_view* grid() const;
    stream_cell* active_cell() const;
    stream_cell* tile_for_stream_name(
        const QString& name, const stream_route_state& route_state
    ) const;

    void initialize_editor_state(const active_edit_session& edit_session) const;
    void sync_active_line_editor(
        const active_edit_session& edit_session, bool reset_form = false
    ) const;
    void sync_active_template_editor(
        const active_edit_session& edit_session, bool reset_form = false
    ) const;
    void add_template_candidate(const QString& name) const;
    void register_stream_entry(
        const QString& final_name, const QString& source_desc
    ) const;
    stream_cell* show_stream_in_grid(
        const QString& name, const stream_settings& settings_value,
        const active_edit_session& edit_session,
        const grid_stream_binding& binding
    ) const;
    void hide_stream_from_grid(const QString& name, bool clear_active) const;
    void sync_active_candidates() const;
    void sync_visible_log_mode(app_log_mode mode) const;
    void sync_active_selection(
        const QString& active_name, const stream_settings& settings_value
    ) const;
    void sync_stream_visual_settings(
        const QString& name, const stream_settings& settings_value,
        const stream_route_state& route_state
    ) const;
    void apply_active_stream(
        const QString& active_name, const stream_settings& settings_value,
        const active_edit_session& edit_session
    ) const;
    void sync_active_persistent(
        const QString& active_name, const active_edit_session& edit_session
    ) const;
    void apply_template_preview(
        const QString& template_name, const active_edit_session& edit_session
    ) const;

private:
    active_editor_bridge editor_bridge_;
    settings_panel* settings_ { nullptr };
    stream_board* main_zone_ { nullptr };
    grid_view* grid_ { nullptr };
};

#endif // YODAU_APP_SHELL_STREAM_WIDGET_BRIDGE_HPP
