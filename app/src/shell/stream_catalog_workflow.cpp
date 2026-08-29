#include "shell/stream_catalog_workflow.hpp"

#include "core/namespace_alias.hpp"
#include "shell/str_label.hpp"
#include "shell/stream_catalog_state.hpp"
#include "shell/stream_route_state.hpp"
#include "shell/stream_widget_bridge.hpp"
#include "streams/stream.hpp"
#include "streams/stream_manager.hpp"
#include "widgets/settings_panel.hpp"

#include <QCameraDevice>
#include <QMediaDevices>
#include <QSet>
#include <utility>

namespace {

QStringList local_source_names(const yodau::core::stream_manager& manager) {
    QStringList names;
    for (const std::string& name : manager.detected_local_stream_names()) {
        names.push_back(QString::fromStdString(name));
    }
    return names;
}

} // namespace

stream_catalog_workflow::stream_catalog_workflow(
    yodau::core::stream_manager* stream_mgr, settings_panel* settings,
    stream_catalog_state& catalog_state, stream_widget_bridge& widget_bridge
)
    : stream_mgr_(stream_mgr)
    , settings_(settings)
    , catalog_state_(catalog_state)
    , widget_bridge_(widget_bridge) { }

void stream_catalog_workflow::seed_from_core() const {
    if (stream_mgr_ == nullptr) {
        return;
    }

    for (const std::string& name : stream_mgr_->stream_names()) {
        const QString qname = QString::fromStdString(name);
        catalog_state_.ensure_stream(qname);
        register_stream_in_ui(
            qname, describe_stream_source(*stream_mgr_, name)
        );
    }
}

stream_catalog_workflow::transition_result
stream_catalog_workflow::detect_local_sources() const {
    transition_result transition;
    if (stream_mgr_ == nullptr) {
        return transition;
    }

    const QStringList previous_locals = local_source_names(*stream_mgr_);
    stream_mgr_->refresh_local_streams();

    const QStringList locals = local_source_names(*stream_mgr_);
    for (const QString& previous_name : previous_locals) {
        if (locals.contains(previous_name)) {
            continue;
        }
        catalog_state_.remove_stream(previous_name);
        widget_bridge_.unregister_stream_entry(previous_name);
        transition.removed_streams.push_back(previous_name);
    }
    const auto cameras = QMediaDevices::videoInputs();
    QList<local_source_descriptor> source_descriptors;
    QSet<QString> source_ids;
    for (const QString& local_name : locals) {
        const auto local_stream
            = stream_mgr_->find_stream(local_name.toStdString());
        const QString source_id = local_stream
            ? QString::fromStdString(local_stream->get_path())
            : local_name;
        if (source_id.isEmpty() || source_ids.contains(source_id)) {
            continue;
        }
        source_ids.insert(source_id);
        source_descriptors.push_back(
            local_source_descriptor {
                .id = source_id,
                .display_name = local_name == source_id
                    ? local_name
                    : QStringLiteral("%1 — %2").arg(local_name, source_id),
            }
        );
    }
    for (const QCameraDevice& camera : cameras) {
        const QString source_id = QString::fromUtf8(camera.id()).trimmed();
        if (source_id.isEmpty() || source_ids.contains(source_id)) {
            continue;
        }
        source_ids.insert(source_id);
        const QString description = camera.description().trimmed();
        source_descriptors.push_back(
            local_source_descriptor {
                .id = source_id,
                .display_name = description.isEmpty()
                    ? source_id
                    : QStringLiteral("%1 — %2").arg(description, source_id),
            }
        );
    }
    if (settings_ != nullptr) {
        settings_->set_local_sources(source_descriptors);
    }
    transition.entries.push_back(make_add_entry(
        app_log_severity::info, QStringLiteral("local_sources"),
        QStringLiteral("local source inventory refreshed"), QString(),
        QStringLiteral("core=%1 qt=%2").arg(locals.size()).arg(cameras.size())
    ));
    transition.update_monitor_inventory = true;
    transition.monitor_marker = QStringLiteral("local_sources_refreshed");

    return transition;
}

stream_catalog_workflow::transition_result stream_catalog_workflow::add_stream(
    const QString& source, const QString& name, const QString& type,
    const bool loop
) const {
    transition_result transition;
    if (stream_mgr_ == nullptr) {
        return transition;
    }

    const stream_route_state::add_source_validation validation
        = stream_route_state::validate_add_source(source, type);
    if (!validation.valid) {
        transition.entries.push_back(make_add_entry(
            app_log_severity::warning, QStringLiteral("stream_add"),
            validation.message, name, validation.detail
        ));
        return transition;
    }

    try {
        const auto& stream = stream_mgr_->add_stream(
            source.toStdString(), name.toStdString(), type.toStdString(), loop
        );

        const QString final_name = QString::fromStdString(stream.get_name());
        const QString source_desc
            = stream_route_state::source_description(source, type);

        transition.entries.push_back(make_add_entry(
            app_log_severity::info, QStringLiteral("stream_add"),
            QStringLiteral("stream added"), final_name, source_desc
        ));

        register_stream_in_ui(final_name, source_desc);
        transition.added_stream = final_name;
        transition.refresh_fps = true;
        transition.update_monitor_inventory = true;
        transition.monitor_marker = QStringLiteral("stream_added");
        return transition;
    } catch (const std::exception& error) {
        transition.entries.push_back(make_add_entry(
            app_log_severity::error, QStringLiteral("stream_add"),
            QStringLiteral("add %1 stream failed").arg(type), name,
            QString::fromLocal8Bit(error.what())
        ));
        return transition;
    }
}

app_log_entry stream_catalog_workflow::make_add_entry(
    const app_log_severity severity, const QString& subsystem,
    const QString& message, const QString& stream_name, const QString& detail
) {
    return app_log_entry {
        .timestamp = QDateTime(),
        .area = app_log_area::add,
        .severity = severity,
        .subsystem = subsystem,
        .stream_name = stream_name,
        .line_name = QString(),
        .algorithm_id = QString(),
        .event_type = QString(),
        .message = message,
        .detail = detail,
        .line_color = QColor(),
    };
}

QString stream_catalog_workflow::unknown_source_description() {
    return str_label("<unknown>");
}

QString stream_catalog_workflow::describe_stream_source(
    const yodau::core::stream_manager& stream_mgr, const std::string& name
) {
    const auto stream_ptr = stream_mgr.find_stream(name);
    if (!stream_ptr) {
        return unknown_source_description();
    }

    const QString path = QString::fromStdString(stream_ptr->get_path());
    const QString type = QString::fromStdString(
        yodau::core::stream::type_name(stream_ptr->get_type())
    );
    return QStringLiteral("%1:%2").arg(type, path);
}

void stream_catalog_workflow::register_stream_in_ui(
    const QString& final_name, const QString& source_desc
) const {
    catalog_state_.ensure_stream(final_name);
    widget_bridge_.register_stream_entry(final_name, source_desc);
}
