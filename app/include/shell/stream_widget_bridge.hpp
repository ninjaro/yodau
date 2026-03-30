#ifndef YODAU_FRONTEND_SHELL_STREAM_WIDGET_BRIDGE_HPP
#define YODAU_FRONTEND_SHELL_STREAM_WIDGET_BRIDGE_HPP

#include "shell/frontend_settings.hpp"

#include <QString>

class active_edit_session;
class grid_view;
class settings_panel;
class stream_board;
class stream_cell;
class stream_route_state;

class stream_widget_bridge final {
public:
    stream_widget_bridge(
        stream_board* main_zone = nullptr, settings_panel* settings = nullptr
    );

    grid_view* grid() const;
    stream_cell* active_cell() const;
    stream_cell* tile_for_stream_name(
        const QString& name, const stream_route_state& route_state
    ) const;

    void initialize_editor_state(const active_edit_session& edit_session) const;
    void sync_active_candidates() const;
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
    settings_panel* settings_ { nullptr };
    stream_board* main_zone_ { nullptr };
    grid_view* grid_ { nullptr };
};

#endif // YODAU_FRONTEND_SHELL_STREAM_WIDGET_BRIDGE_HPP
