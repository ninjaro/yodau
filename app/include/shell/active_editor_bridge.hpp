#ifndef YODAU_APP_SHELL_ACTIVE_EDITOR_BRIDGE_HPP
#define YODAU_APP_SHELL_ACTIVE_EDITOR_BRIDGE_HPP

#include "shell/app_settings.hpp"

#include <QString>

class active_edit_session;
class grid_view;
class settings_panel;
class stream_board;
class stream_cell;

class active_editor_bridge final {
public:
    explicit active_editor_bridge(
        stream_board* main_zone = nullptr, settings_panel* settings = nullptr
    );

    void initialize_editor_state(const active_edit_session& edit_session) const;
    void sync_active_line_editor(
        const active_edit_session& edit_session, bool reset_form = false
    ) const;
    void sync_active_template_editor(
        const active_edit_session& edit_session, bool reset_form = false
    ) const;
    void add_template_candidate(const QString& name) const;
    void sync_active_candidates() const;
    void sync_active_selection(
        const QString& active_name, const stream_settings& settings_value
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

#endif // YODAU_APP_SHELL_ACTIVE_EDITOR_BRIDGE_HPP
