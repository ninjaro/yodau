#ifndef YODAU_FRONTEND_SHELL_STREAM_CATALOG_WORKFLOW_HPP
#define YODAU_FRONTEND_SHELL_STREAM_CATALOG_WORKFLOW_HPP

#include "shell/frontend_log.hpp"

#include <QString>
#include <QVector>

#include <string>

namespace yodau::backend {
class stream_manager;
}

class settings_panel;
class stream_catalog_state;
class stream_widget_bridge;

class stream_catalog_workflow final {
public:
    struct transition_result {
        QVector<frontend_log_entry> entries;
        bool refresh_fps { false };
        bool update_monitor_inventory { false };
        QString monitor_marker;
    };

    stream_catalog_workflow(
        yodau::backend::stream_manager* stream_mgr, settings_panel* settings,
        stream_catalog_state& catalog_state, stream_widget_bridge& widget_bridge
    );

    void seed_from_backend() const;

    [[nodiscard]] transition_result detect_local_sources() const;
    [[nodiscard]] transition_result add_stream(
        const QString& source, const QString& name, const QString& type,
        bool loop
    ) const;

private:
    [[nodiscard]] static frontend_log_entry make_add_entry(
        frontend_log_severity severity, const QString& subsystem,
        const QString& message, const QString& stream_name = QString(),
        const QString& detail = QString()
    );
    [[nodiscard]] static QString unknown_source_description();
    [[nodiscard]] static QString describe_stream_source(
        const yodau::backend::stream_manager& stream_mgr, const std::string& name
    );
    void register_stream_in_ui(
        const QString& final_name, const QString& source_desc
    ) const;

    yodau::backend::stream_manager* stream_mgr_ { nullptr };
    settings_panel* settings_ { nullptr };
    stream_catalog_state& catalog_state_;
    stream_widget_bridge& widget_bridge_;
};

#endif // YODAU_FRONTEND_SHELL_STREAM_CATALOG_WORKFLOW_HPP
