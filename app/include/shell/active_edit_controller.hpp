#ifndef YODAU_FRONTEND_SHELL_ACTIVE_EDIT_CONTROLLER_HPP
#define YODAU_FRONTEND_SHELL_ACTIVE_EDIT_CONTROLLER_HPP

#include "shell/frontend_settings.hpp"

class active_edit_session;
class stream_widget_bridge;

class active_edit_controller final {
public:
    active_edit_controller(
        active_edit_session& edit_session, stream_widget_bridge& widget_bridge
    );

    void initialize_editor_state() const;
    void set_drawing_new_mode(bool drawing_new) const;
    const line_profile& apply_line_profile(line_profile profile_value) const;
    const template_apply_settings& apply_template_settings(
        template_apply_settings settings_value
    ) const;
    line_edit_request apply_line_edit_preview(line_edit_request request) const;
    void clear_line_edit_preview() const;
    void reset_after_line_saved(const QString& final_name) const;
    void reset_after_line_edit_saved() const;
    void reset_after_template_applied() const;
    void undo_last_draft_point() const;

private:
    active_edit_session& edit_session_;
    stream_widget_bridge& widget_bridge_;
};

#endif // YODAU_FRONTEND_SHELL_ACTIVE_EDIT_CONTROLLER_HPP
