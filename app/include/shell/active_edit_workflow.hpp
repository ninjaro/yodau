#ifndef YODAU_FRONTEND_SHELL_ACTIVE_EDIT_WORKFLOW_HPP
#define YODAU_FRONTEND_SHELL_ACTIVE_EDIT_WORKFLOW_HPP

#include "shell/frontend_log.hpp"
#include "shell/frontend_settings.hpp"

#include <QString>
#include <QVector>

class active_edit_actions;
class active_edit_controller;
class stream_catalog_state;
class stream_route_state;
class stream_widget_bridge;
class stream_cell;

class active_edit_workflow final {
public:
    struct transition_result {
        QVector<frontend_log_entry> entries;
        bool refresh_fps { false };
        bool update_monitor_inventory { false };
        QString monitor_marker;
    };

    active_edit_workflow(
        const stream_route_state& route_state,
        const stream_catalog_state& catalog_state,
        stream_widget_bridge& widget_bridge,
        active_edit_controller& edit_controller,
        active_edit_actions& edit_actions
    );

    [[nodiscard]] transition_result set_drawing_new_mode(bool drawing_new) const;
    [[nodiscard]] transition_result
    apply_line_profile(line_profile profile_value) const;
    [[nodiscard]] transition_result
    save_active_line(line_profile profile_value) const;
    [[nodiscard]] transition_result apply_template_settings(
        template_apply_settings settings_value
    ) const;
    [[nodiscard]] transition_result
    apply_active_template(template_apply_settings settings_value) const;

    void undo_last_draft_point() const;

private:
    struct active_context {
        QString active_name;
        QString algorithm_id;
    };

    [[nodiscard]] active_context current_context() const;
    [[nodiscard]] transition_result
    active_cell_failure(const QString& fail_prefix) const;
    [[nodiscard]] stream_cell* checked_active_cell(
        const QString& fail_prefix, transition_result& result
    ) const;
    [[nodiscard]] static frontend_log_entry make_active_entry(
        const active_context& context, frontend_log_severity severity,
        const QString& subsystem, const QString& message,
        const QString& detail = QString()
    );

    const stream_route_state& route_state_;
    const stream_catalog_state& catalog_state_;
    stream_widget_bridge& widget_bridge_;
    active_edit_controller& edit_controller_;
    active_edit_actions& edit_actions_;
};

#endif // YODAU_FRONTEND_SHELL_ACTIVE_EDIT_WORKFLOW_HPP
