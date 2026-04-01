#include "shell/active_stream_workflow.hpp"

#include <utility>

namespace active_stream_workflow_support {

QString algorithm_detail_text(const stream_settings& settings_value) {
    return QStringLiteral("backend runtime uses %1; preset=%2 overlay=%3")
        .arg(settings_value.algorithm_id)
        .arg(settings_value.algorithm_preset)
        .arg(
            settings_value.algorithm_overlay_enabled ? QStringLiteral("true")
                                                     : QStringLiteral("false")
        );
}

QString processing_policy_detail_text(const stream_settings& settings_value) {
    return QStringLiteral(
               "manual=%1 display_fps=%2 backend_fps=%3 processing_pixels=%4"
           )
        .arg(
            settings_value.manual_processing_policy_enabled
                ? QStringLiteral("true")
                : QStringLiteral("false")
        )
        .arg(settings_value.manual_display_fps)
        .arg(settings_value.manual_backend_fps)
        .arg(settings_value.manual_processing_pixels);
}

} // namespace active_stream_workflow_support

active_stream_workflow::active_stream_workflow(
    active_stream_state& active_streams
)
    : active_streams_(active_streams) {}

active_stream_workflow::transition_result
active_stream_workflow::set_active_stream(const QString& name) const {
    const active_stream_state::selection_result selection
        = active_streams_.set_active_stream(name);
    return selection_result_entry(selection);
}

active_stream_workflow::transition_result
active_stream_workflow::apply_stream_settings(
    stream_settings settings_value
) const {
    transition_result transition;
    const active_stream_state::settings_result result
        = active_streams_.apply_stream_settings(std::move(settings_value));

    if (result.outcome_value
        == active_stream_state::settings_result::outcome::
            switched_active_stream) {
        return switched_result_entry(result);
    }

    if (result.outcome_value
        == active_stream_state::settings_result::outcome::ignored_empty_stream) {
        return transition;
    }

    if (result.labels_changed) {
        transition.entries.push_back(
            make_entry(
                frontend_log_severity::info, QStringLiteral("stream_settings"),
                result.settings.labels_enabled
                    ? QStringLiteral("line labels enabled")
                    : QStringLiteral("line labels disabled"),
                result.settings.stream_name
            )
        );
    }

    if (result.standard_labels_changed) {
        transition.entries.push_back(
            make_entry(
                frontend_log_severity::info, QStringLiteral("stream_settings"),
                result.settings.standard_labels_enabled
                    ? QStringLiteral("standard labels enabled")
                    : QStringLiteral("standard labels disabled"),
                result.settings.stream_name
            )
        );
    }

    if (result.algorithm_changed) {
        transition.entries.push_back(
            make_entry(
                frontend_log_severity::info,
                QStringLiteral("stream_settings"),
                QStringLiteral("algorithm preference updated"),
                result.settings.stream_name,
                active_stream_workflow_support::algorithm_detail_text(
                    result.settings
                ),
                result.settings.algorithm_id
            )
        );
    }

    if (result.processing_policy_changed) {
        transition.entries.push_back(
            make_entry(
                frontend_log_severity::info,
                QStringLiteral("stream_processing"),
                QStringLiteral("processing tuning updated"),
                result.settings.stream_name,
                active_stream_workflow_support::processing_policy_detail_text(
                    result.settings
                ),
                result.settings.algorithm_id
            )
        );
        transition.refresh_fps = true;
    }

    return transition;
}

frontend_log_entry active_stream_workflow::make_entry(
    const frontend_log_severity severity, const QString& subsystem,
    const QString& message, const QString& stream_name, const QString& detail,
    const QString& algorithm_id
) {
    return frontend_log_entry {
        .timestamp = QDateTime(),
        .area = frontend_log_area::active,
        .severity = severity,
        .subsystem = subsystem,
        .stream_name = stream_name,
        .line_name = QString(),
        .algorithm_id = algorithm_id,
        .event_type = QString(),
        .message = message,
        .detail = detail,
        .line_color = QColor(),
    };
}

active_stream_workflow::transition_result
active_stream_workflow::selection_result_entry(
    const active_stream_state::selection_result& selection
) {
    transition_result transition;
    transition.refresh_fps = true;
    transition.update_monitor_inventory = true;
    transition.monitor_marker = selection.active_name.isEmpty()
        ? QStringLiteral("active_stream_cleared")
        : QStringLiteral("active_stream_selected");

    transition.entries.push_back(
        make_entry(
            frontend_log_severity::info, QStringLiteral("active_stream"),
            selection.active_name.isEmpty()
                ? QStringLiteral("active stream cleared")
                : QStringLiteral("active stream selected"),
            selection.active_name, QString(), selection.settings.algorithm_id
        )
    );

    return transition;
}

active_stream_workflow::transition_result
active_stream_workflow::switched_result_entry(
    const active_stream_state::settings_result& result
) {
    return selection_result_entry(
        active_stream_state::selection_result {
            .active_name = result.active_name,
            .settings = result.settings,
            .changed = result.active_name != result.previous_active_name,
        }
    );
}
