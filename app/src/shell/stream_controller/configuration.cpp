#include "configuration/line_configuration_file.hpp"
#include "configuration/line_configuration_json.hpp"
#include "geometry/geometry.hpp"
#include "shell/stream_controller.hpp"
#include "streams/stream.hpp"
#include "widgets/settings_panel.hpp"
#include "widgets/stream_cell.hpp"

#include <QColor>
#include <string_view>
#include <utility>

namespace stream_controller_support {

std::vector<yodau::core::point>
core_points_from_app(const std::vector<QPointF>& points) {
    std::vector<yodau::core::point> converted;
    converted.reserve(points.size());
    for (const QPointF& point_value : points) {
        converted.push_back(
            yodau::core::point {
                .x = static_cast<float>(point_value.x()),
                .y = static_cast<float>(point_value.y()),
            }
        );
    }
    return converted;
}

std::vector<QPointF>
app_points_from_core(const std::vector<yodau::core::point>& points) {
    std::vector<QPointF> converted;
    converted.reserve(points.size());
    for (const yodau::core::point& point_value : points) {
        converted.emplace_back(point_value.x, point_value.y);
    }
    return converted;
}

QString configuration_error_text(const std::exception& error) {
    return QString::fromUtf8(error.what());
}

} // namespace stream_controller_support

QString stream_controller::active_configuration_stream_name() const {
    QString active_name = route_state.active_stream_name().trimmed();
    if (!active_name.isEmpty()) {
        return active_name;
    }
    return settings != nullptr
        ? settings->current_active_stream_settings().stream_name.trimmed()
        : QString();
}

bool stream_controller::build_line_configuration_document(
    yodau::core::line_configuration_document* document, QString* error_message
) const {
    if (document == nullptr) {
        if (error_message != nullptr) {
            *error_message = tr("No configuration document was provided.");
        }
        return false;
    }
    const QString stream_name = active_configuration_stream_name();
    if (stream_mgr == nullptr || stream_name.isEmpty()) {
        if (error_message != nullptr) {
            *error_message
                = tr("Select an active stream before exporting lines.");
        }
        return false;
    }

    const auto stream_value
        = stream_mgr->find_stream(stream_name.toStdString());
    if (!stream_value) {
        if (error_message != nullptr) {
            *error_message = tr("The active stream no longer exists.");
        }
        return false;
    }

    try {
        const stream_settings app_settings_value
            = catalog_state.settings_for(stream_name);
        const stream_runtime_metrics runtime_metrics
            = runtime_metrics_by_stream.value(stream_name);
        const int configured_core_fps
            = app_settings_value.manual_processing_policy_enabled
            ? app_settings_value.manual_core_fps
            : (runtime_metrics.effective_core_fps > 0
                   ? runtime_metrics.effective_core_fps
                   : default_manual_core_fps());

        yodau::core::line_configuration_document built;
        built.stream.name = stream_name.toStdString();
        built.stream.source = stream_value->get_path();
        built.stream.type
            = yodau::core::stream::type_name(stream_value->get_type());
        built.stream.loop = stream_value->is_looping();
        built.stream.virtual_camera_path
            = virtual_camera_path_by_stream
                  .value(stream_name, QStringLiteral("/dev/yodau0"))
                  .toStdString();
        built.stream.analysis_interval_ms
            = interval_ms_for_fps(configured_core_fps);
        built.stream.algorithm
            = core_runtime.algorithm_settings_for_stream(built.stream.name);
        built.stream.labels_enabled = app_settings_value.labels_enabled;
        built.stream.standard_labels_enabled
            = app_settings_value.standard_labels_enabled;
        built.stream.movement_display_mode
            = app_settings_value.movement_display_mode.toStdString();
        built.stream.manual_processing_policy_enabled
            = app_settings_value.manual_processing_policy_enabled;
        built.stream.manual_display_fps = app_settings_value.manual_display_fps;
        built.stream.manual_core_fps = app_settings_value.manual_core_fps;
        built.stream.manual_processing_pixels
            = app_settings_value.manual_processing_pixels;

        const auto& app_lines = edit_session.stream_lines(stream_name);
        built.lines.reserve(app_lines.size());
        for (const stream_cell::line_instance& app_line : app_lines) {
            const std::string line_name = app_line.template_name.toStdString();
            yodau::core::configured_line configured;
            configured.name = line_name;
            configured.points = stream_controller_support::core_points_from_app(
                app_line.pts_pct
            );
            configured.closed = app_line.closed;
            configured.direction
                = stream_mgr->find_line_direction(line_name).value_or(
                    yodau::core::tripwire_dir::any
                );
            configured.enabled = app_line.enabled;
            configured.profile
                = stream_mgr
                      ->find_stream_line_profile(built.stream.name, line_name)
                      .value_or(
                          stream_mgr->find_line_profile(line_name).value_or(
                              yodau::core::make_line_profile(line_name)
                          )
                      );
            configured.appearance.color = app_line.color.isValid()
                ? app_line.color.name(QColor::HexArgb).toStdString()
                : std::string("#ffff0000");
            configured.appearance.color_mode
                = normalized_line_color_mode_id(app_line.color_mode_id)
                      .toStdString();
            configured.appearance.width_text
                = normalized_line_width_text(app_line.width_text).toStdString();
            configured.appearance.length_text
                = normalized_line_length_text(app_line.length_text)
                      .toStdString();
            configured.appearance.response_text
                = normalized_line_response_text(app_line.response_text)
                      .toStdString();
            built.lines.push_back(std::move(configured));
        }
        yodau::core::validate_line_configuration(built);
        *document = std::move(built);
        return true;
    } catch (const std::exception& error) {
        if (error_message != nullptr) {
            *error_message
                = stream_controller_support::configuration_error_text(error);
        }
        return false;
    }
}

bool stream_controller::export_line_configuration_to(
    const QString& path, QString* error_message
) const {
    yodau::core::line_configuration_document document;
    if (!build_line_configuration_document(&document, error_message)) {
        return false;
    }

    try {
        yodau::core::save_line_configuration_file_atomic(
            document, std::filesystem::path(path.toStdString())
        );
        return true;
    } catch (const std::exception& error) {
        if (error_message != nullptr) {
            *error_message
                = stream_controller_support::configuration_error_text(error);
        }
        return false;
    }
}

bool stream_controller::export_line_configuration_data(
    QByteArray* contents, QString* error_message
) const {
    if (contents == nullptr) {
        if (error_message != nullptr) {
            *error_message = tr("No configuration output was provided.");
        }
        return false;
    }
    contents->clear();
    yodau::core::line_configuration_document document;
    if (!build_line_configuration_document(&document, error_message)) {
        return false;
    }

    try {
        const std::string serialized
            = yodau::core::encode_line_configuration_json(document);
        *contents = QByteArray(
            serialized.data(), static_cast<qsizetype>(serialized.size())
        );
        return true;
    } catch (const std::exception& error) {
        if (error_message != nullptr) {
            *error_message
                = stream_controller_support::configuration_error_text(error);
        }
        return false;
    }
}

bool stream_controller::import_line_configuration_from(
    const QString& path, QString* error_message
) {
    try {
        const auto document = yodau::core::load_line_configuration_file(
            std::filesystem::path(path.toStdString())
        );
        return apply_line_configuration_document(
            document, error_message, nullptr
        );
    } catch (const std::exception& error) {
        if (error_message != nullptr) {
            *error_message
                = stream_controller_support::configuration_error_text(error);
        }
        return false;
    }
}

bool stream_controller::import_line_configuration_data(
    const QByteArray& contents, QString* error_message,
    QString* imported_stream_name
) {
    try {
        const auto document = yodau::core::decode_line_configuration_json(
            std::string_view(
                contents.constData(), static_cast<size_t>(contents.size())
            )
        );
        return apply_line_configuration_document(
            document, error_message, imported_stream_name
        );
    } catch (const std::exception& error) {
        if (error_message != nullptr) {
            *error_message
                = stream_controller_support::configuration_error_text(error);
        }
        return false;
    }
}

bool stream_controller::apply_line_configuration_document(
    const yodau::core::line_configuration_document& document,
    QString* error_message, QString* imported_stream_name
) {
    if (stream_mgr == nullptr) {
        if (error_message != nullptr) {
            *error_message = tr("The processing runtime is not available.");
        }
        return false;
    }

    try {
        const yodau::core::line_configuration_apply_options options;
        // File -> Import always honors the stream identity in the shared
        // document, matching headless-daemon behavior. Applying a camera A
        // configuration to whichever tile happens to be active would be a
        // surprising and potentially unsafe implicit mutation.
        const std::string resolved_name = document.stream.name;
        const bool stream_already_registered
            = stream_mgr->find_stream(resolved_name) != nullptr;
        const auto result = yodau::core::apply_line_configuration(
            document, *stream_mgr, core_runtime, options
        );
        const QString stream_name = QString::fromStdString(result.stream_name);
        if (imported_stream_name != nullptr) {
            *imported_stream_name = stream_name;
        }
        virtual_camera_path_by_stream.insert(
            stream_name, QString::fromStdString(result.virtual_camera_path)
        );

        catalog_state.ensure_stream(stream_name);
        stream_settings imported_settings {
            .stream_name = stream_name,
            .labels_enabled = document.stream.labels_enabled,
            .standard_labels_enabled = document.stream.standard_labels_enabled,
            .algorithm_id
            = QString::fromStdString(document.stream.algorithm.algorithm_id),
            .algorithm_preset
            = QString::fromStdString(document.stream.algorithm.preset_id),
            .movement_display_mode
            = QString::fromStdString(document.stream.movement_display_mode),
            .algorithm_overlay_enabled = movement_display_enabled(
                QString::fromStdString(document.stream.movement_display_mode)
            ),
            .manual_processing_policy_enabled
            = document.stream.manual_processing_policy_enabled,
            .manual_display_fps = document.stream.manual_display_fps,
            .manual_core_fps = document.stream.manual_core_fps,
            .manual_processing_pixels
            = document.stream.manual_processing_pixels,
        };
        imported_settings = stream_catalog_state::normalized_stream_settings(
            std::move(imported_settings)
        );
        catalog_state.set_stream_settings(imported_settings);

        if (!stream_already_registered) {
            widget_bridge.register_stream_entry(
                stream_name,
                QStringLiteral("%1:%2")
                    .arg(QString::fromStdString(document.stream.type))
                    .arg(QString::fromStdString(result.source))
            );
        }

        std::vector<stream_cell::line_instance> imported_lines;
        imported_lines.reserve(document.lines.size());
        for (const yodau::core::configured_line& configured : document.lines) {
            stream_cell::line_instance app_line;
            app_line.template_name = QString::fromStdString(configured.name);
            app_line.color
                = QColor(QString::fromStdString(configured.appearance.color));
            app_line.color_mode_id = normalized_line_color_mode_id(
                QString::fromStdString(configured.appearance.color_mode)
            );
            app_line.enabled = configured.enabled;
            app_line.closed = configured.closed;
            app_line.width_text = normalized_line_width_text(
                QString::fromStdString(configured.appearance.width_text)
            );
            app_line.length_text = normalized_line_length_text(
                QString::fromStdString(configured.appearance.length_text)
            );
            app_line.response_text = normalized_line_response_text(
                QString::fromStdString(configured.appearance.response_text)
            );
            app_line.pts_pct = stream_controller_support::app_points_from_core(
                configured.points
            );
            imported_lines.push_back(std::move(app_line));
        }
        edit_session.replace_stream_lines(
            stream_name, std::move(imported_lines)
        );

        widget_bridge.sync_stream_visual_settings(
            stream_name, imported_settings, route_state
        );
        if (route_state.is_active_stream(stream_name)) {
            widget_bridge.apply_active_stream(
                stream_name, imported_settings, edit_session
            );
        } else if (
            auto* tile
            = widget_bridge.tile_for_stream_name(stream_name, route_state)
        ) {
            tile->set_persistent_lines(edit_session.stream_lines(stream_name));
        }
        widget_bridge.sync_active_candidates();
        refresh_fps_policy(true);
        append_log(
            app_log_area::active, app_log_severity::info,
            QStringLiteral("line_configuration"),
            QStringLiteral("line configuration imported"), stream_name,
            QStringLiteral("%1 lines from shared configuration data")
                .arg(document.lines.size())
        );
        return true;
    } catch (const std::exception& error) {
        if (error_message != nullptr) {
            *error_message
                = stream_controller_support::configuration_error_text(error);
        }
        return false;
    }
}
