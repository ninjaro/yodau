#ifndef YODAU_APP_SHELL_STREAM_CATALOG_WORKFLOW_HPP
#define YODAU_APP_SHELL_STREAM_CATALOG_WORKFLOW_HPP

#include "shell/app_log.hpp"

#include <QString>
#include <QStringList>
#include <QVector>

#include <string>

namespace yodau::core {
class stream_manager;
}

class settings_panel;
class stream_catalog_state;
class stream_widget_bridge;

class stream_catalog_workflow final {
public:
    struct transition_result {
        QVector<app_log_entry> entries;
        QStringList removed_streams;
        QString added_stream;
        bool refresh_fps { false };
    };

    stream_catalog_workflow(
        yodau::core::stream_manager* stream_mgr, settings_panel* settings,
        stream_catalog_state& catalog_state, stream_widget_bridge& widget_bridge
    );

    void seed_from_core() const;

    [[nodiscard]] transition_result detect_local_sources() const;
    [[nodiscard]] transition_result add_stream(
        const QString& source, const QString& name, const QString& type,
        bool loop
    ) const;

private:
    [[nodiscard]] static app_log_entry make_add_entry(
        app_log_severity severity, const QString& subsystem,
        const QString& message, const QString& stream_name = QString(),
        const QString& detail = QString()
    );
    [[nodiscard]] static QString unknown_source_description();
    [[nodiscard]] static QString describe_stream_source(
        const yodau::core::stream_manager& stream_mgr, const std::string& name
    );
    void register_stream_in_ui(
        const QString& final_name, const QString& source_desc
    ) const;

    yodau::core::stream_manager* stream_mgr_ { nullptr };
    settings_panel* settings_ { nullptr };
    stream_catalog_state& catalog_state_;
    stream_widget_bridge& widget_bridge_;
};

#endif // YODAU_APP_SHELL_STREAM_CATALOG_WORKFLOW_HPP
