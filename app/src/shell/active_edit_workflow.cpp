#include "shell/active_edit_workflow.hpp"

#include "shell/active_edit_actions.hpp"
#include "shell/active_edit_controller.hpp"
#include "shell/stream_catalog_state.hpp"
#include "shell/stream_route_state.hpp"
#include "shell/stream_widget_bridge.hpp"
#include "widgets/stream_cell.hpp"

namespace active_edit_workflow_support {

QString line_profile_detail(const line_profile& profile_value) {
    return QStringLiteral("name=%1 color=%2 closed=%3 %4")
        .arg(profile_value.name)
        .arg(profile_value.color.name())
        .arg(
            profile_value.closed ? QStringLiteral("true")
                                 : QStringLiteral("false")
        )
        .arg(
            line_profile_summary_text(
                profile_value.width_text, profile_value.length_text,
                profile_value.response_text
            )
        );
}

QString line_save_request_detail(const line_profile& profile_value) {
    return QStringLiteral("name=%1 closed=%2 %3")
        .arg(profile_value.name)
        .arg(
            profile_value.closed ? QStringLiteral("true")
                                 : QStringLiteral("false")
        )
        .arg(
            line_profile_summary_text(
                profile_value.width_text, profile_value.length_text,
                profile_value.response_text
            )
        );
}

QString line_added_detail(const active_edit_actions::line_save_result& result) {
    return QStringLiteral("template=%1 points=%2 closed=%3 %4")
        .arg(result.final_name)
        .arg(result.point_count)
        .arg(
            result.line.closed ? QStringLiteral("true")
                               : QStringLiteral("false")
        )
        .arg(
            line_profile_summary_text(
                result.line.width_text, result.line.length_text,
                result.line.response_text
            )
        );
}

QString template_settings_detail(
    const template_apply_settings& settings_value
) {
    return QStringLiteral("template=%1 color=%2 %3")
        .arg(settings_value.template_name)
        .arg(settings_value.color.name())
        .arg(
            line_profile_summary_text(
                settings_value.width_text, settings_value.length_text,
                settings_value.response_text
            )
        );
}

QString template_applied_detail(
    const active_edit_actions::template_apply_result& result
) {
    return QStringLiteral("%1 %2")
        .arg(result.settings.template_name)
        .arg(
            line_profile_summary_text(
                result.line.width_text, result.line.length_text,
                result.line.response_text
            )
        );
}

} // namespace active_edit_workflow_support

active_edit_workflow::active_edit_workflow(
    const stream_route_state& route_state,
    const stream_catalog_state& catalog_state,
    stream_widget_bridge& widget_bridge,
    active_edit_controller& edit_controller,
    active_edit_actions& edit_actions
)
    : route_state_(route_state)
    , catalog_state_(catalog_state)
    , widget_bridge_(widget_bridge)
    , edit_controller_(edit_controller)
    , edit_actions_(edit_actions) {}

active_edit_workflow::transition_result
active_edit_workflow::set_drawing_new_mode(const bool drawing_new) const {
    transition_result result;
    const active_context context = current_context();

    edit_controller_.set_drawing_new_mode(drawing_new);
    result.entries.push_back(
        make_active_entry(
            context, frontend_log_severity::info, QStringLiteral("editing"),
            drawing_new ? QStringLiteral("edit mode set to draw new")
                        : QStringLiteral("edit mode set to use template")
        )
    );

    return result;
}

active_edit_workflow::transition_result
active_edit_workflow::apply_line_profile(line_profile profile_value) const {
    transition_result result;
    const active_context context = current_context();
    const line_profile& draft_line_profile
        = edit_controller_.apply_line_profile(std::move(profile_value));

    result.entries.push_back(
        make_active_entry(
            context, frontend_log_severity::debug,
            QStringLiteral("line_editor"),
            QStringLiteral("active line draft updated"),
            active_edit_workflow_support::line_profile_detail(draft_line_profile)
        )
    );

    return result;
}

active_edit_workflow::transition_result
active_edit_workflow::save_active_line(line_profile profile_value) const {
    transition_result result;
    const active_context context = current_context();
    stream_cell* cell = checked_active_cell(QStringLiteral("add line"), result);
    if (cell == nullptr) {
        return result;
    }

    const auto action_result = edit_actions_.save_active_line(
        context.active_name, std::move(profile_value), *cell
    );

    result.entries.push_back(
        make_active_entry(
            context, frontend_log_severity::debug,
            QStringLiteral("line_editor"), QStringLiteral("line save requested"),
            active_edit_workflow_support::line_save_request_detail(
                action_result.profile
            )
        )
    );

    if (action_result.status
        == active_edit_actions::line_save_status::insufficient_points) {
        result.entries.push_back(
            make_active_entry(
                context, frontend_log_severity::warning,
                QStringLiteral("line_editor"),
                QStringLiteral("line add requires at least 2 points")
            )
        );
        return result;
    }

    if (!action_result.points_text.isEmpty()) {
        result.entries.push_back(
            make_active_entry(
                context, frontend_log_severity::debug,
                QStringLiteral("line_editor"),
                QStringLiteral("line draft points prepared"),
                action_result.points_text
            )
        );
    }

    if (action_result.status == active_edit_actions::line_save_status::backend_error) {
        result.entries.push_back(
            make_active_entry(
                context, frontend_log_severity::error,
                QStringLiteral("line_editor"), QStringLiteral("line add failed"),
                action_result.error_detail
            )
        );
        return result;
    }

    result.entries.push_back(
        make_active_entry(
            context, frontend_log_severity::info, QStringLiteral("line_editor"),
            QStringLiteral("line added"),
            active_edit_workflow_support::line_added_detail(action_result)
        )
    );
    result.refresh_fps = true;
    result.update_monitor_inventory = true;
    result.monitor_marker = QStringLiteral("line_added");

    return result;
}

active_edit_workflow::transition_result
active_edit_workflow::apply_template_settings(
    template_apply_settings settings_value
) const {
    transition_result result;
    const active_context context = current_context();
    const template_apply_settings& active_template_settings
        = edit_controller_.apply_template_settings(std::move(settings_value));

    result.entries.push_back(
        make_active_entry(
            context, frontend_log_severity::debug,
            QStringLiteral("template_editor"),
            QStringLiteral("template preview updated"),
            active_edit_workflow_support::template_settings_detail(
                active_template_settings
            )
        )
    );

    return result;
}

active_edit_workflow::transition_result
active_edit_workflow::apply_active_template(
    template_apply_settings settings_value
) const {
    transition_result result;
    const active_context context = current_context();
    stream_cell* cell = checked_active_cell(
        QStringLiteral("add template"), result
    );
    if (cell == nullptr) {
        return result;
    }

    const auto action_result = edit_actions_.apply_active_template(
        context.active_name, std::move(settings_value), *cell
    );

    if (action_result.status
        == active_edit_actions::template_apply_status::unknown_template) {
        result.entries.push_back(
            make_active_entry(
                context, frontend_log_severity::warning,
                QStringLiteral("template_editor"),
                QStringLiteral("template add failed: unknown template"),
                action_result.settings.template_name
            )
        );
        return result;
    }

    if (action_result.status
        == active_edit_actions::template_apply_status::backend_error) {
        result.entries.push_back(
            make_active_entry(
                context, frontend_log_severity::error,
                QStringLiteral("template_editor"),
                QStringLiteral("template add failed"),
                action_result.error_detail
            )
        );
        return result;
    }

    result.entries.push_back(
        make_active_entry(
            context, frontend_log_severity::info,
            QStringLiteral("template_editor"),
            QStringLiteral("template added to active stream"),
            active_edit_workflow_support::template_applied_detail(action_result)
        )
    );
    result.refresh_fps = true;

    return result;
}

void active_edit_workflow::undo_last_draft_point() const {
    edit_controller_.undo_last_draft_point();
}

active_edit_workflow::active_context active_edit_workflow::current_context(
) const {
    const QString active_name = route_state_.active_stream_name();
    return active_context {
        .active_name = active_name,
        .algorithm_id = active_name.isEmpty()
            ? QString()
            : catalog_state_.algorithm_id_for(active_name),
    };
}

active_edit_workflow::transition_result
active_edit_workflow::active_cell_failure(const QString& fail_prefix) const {
    transition_result result;
    const active_context context = current_context();

    if (!route_state_.has_active_stream()) {
        result.entries.push_back(
            make_active_entry(
                context, frontend_log_severity::warning,
                QStringLiteral("active_stream"),
                QStringLiteral("%1 failed: no active stream").arg(fail_prefix)
            )
        );
        return result;
    }

    result.entries.push_back(
        make_active_entry(
            context, frontend_log_severity::warning,
            QStringLiteral("active_stream"),
            QStringLiteral("%1 failed: active cell not found").arg(fail_prefix)
        )
    );
    return result;
}

stream_cell* active_edit_workflow::checked_active_cell(
    const QString& fail_prefix, transition_result& result
) const {
    if (!route_state_.has_active_stream()) {
        result = active_cell_failure(fail_prefix);
        return nullptr;
    }

    auto* cell = widget_bridge_.active_cell();
    if (cell == nullptr) {
        result = active_cell_failure(fail_prefix);
        return nullptr;
    }

    return cell;
}

frontend_log_entry active_edit_workflow::make_active_entry(
    const active_context& context, const frontend_log_severity severity,
    const QString& subsystem, const QString& message, const QString& detail
) {
    return frontend_log_entry {
        .timestamp = QDateTime(),
        .area = frontend_log_area::active,
        .severity = severity,
        .subsystem = subsystem,
        .stream_name = context.active_name,
        .algorithm_id = context.algorithm_id,
        .message = message,
        .detail = detail,
    };
}
