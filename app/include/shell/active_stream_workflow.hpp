#ifndef YODAU_FRONTEND_SHELL_ACTIVE_STREAM_WORKFLOW_HPP
#define YODAU_FRONTEND_SHELL_ACTIVE_STREAM_WORKFLOW_HPP

#include "shell/active_stream_state.hpp"
#include "shell/frontend_log.hpp"
#include "shell/frontend_settings.hpp"

#include <QVector>

class active_stream_workflow final {
public:
    struct transition_result {
        QVector<frontend_log_entry> entries;
        bool refresh_fps { false };
        bool update_monitor_inventory { false };
        QString monitor_marker;
    };

    explicit active_stream_workflow(active_stream_state& active_streams);

    [[nodiscard]] transition_result set_active_stream(const QString& name) const;
    [[nodiscard]] transition_result apply_stream_settings(
        stream_settings settings_value
    ) const;

private:
    [[nodiscard]] static frontend_log_entry make_entry(
        frontend_log_severity severity, const QString& subsystem,
        const QString& message, const QString& stream_name = QString(),
        const QString& detail = QString(),
        const QString& algorithm_id = QString()
    );
    [[nodiscard]] static transition_result selection_result_entry(
        const active_stream_state::selection_result& selection
    );
    [[nodiscard]] static transition_result switched_result_entry(
        const active_stream_state::settings_result& result
    );

    active_stream_state& active_streams_;
};

#endif // YODAU_FRONTEND_SHELL_ACTIVE_STREAM_WORKFLOW_HPP
