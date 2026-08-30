#include "shell/stream_controller.hpp"
#include "configuration/line_configuration_file.hpp"
#include "configuration/line_configuration_json.hpp"
#include "core/namespace_alias.hpp"
#include "geometry/geometry.hpp"
#include "streams/stream.hpp"
#include "widgets/settings_panel.hpp"

#include <QCameraDevice>
#include <QColor>
#include <QDateTime>
#include <QImage>
#include <QMetaObject>
#include <QMetaType>
#include <QStringList>
#include <QThread>
#include <QtGlobal>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <optional>
#include <string_view>
#include <utility>

#include "streams/event.hpp"
#include "streams/frame.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/stream_board.hpp"
#include "widgets/stream_cell.hpp"

Q_DECLARE_METATYPE(yodau::core::event)

namespace stream_controller_support {

using steady_clock = std::chrono::steady_clock;

constexpr auto fps_policy_refresh_interval = std::chrono::milliseconds(250);

QPointF app_point_from_core(const yodau::core::point& point_value) {
    return { point_value.x, point_value.y };
}

stream_cell::processing_overlay_kind
app_overlay_kind(const yodau::core::processing_overlay_kind kind) {
    switch (kind) {
    case yodau::core::processing_overlay_kind::point:
        return stream_cell::processing_overlay_kind::point;
    case yodau::core::processing_overlay_kind::polyline:
        return stream_cell::processing_overlay_kind::polyline;
    case yodau::core::processing_overlay_kind::polygon:
        return stream_cell::processing_overlay_kind::polygon;
    case yodau::core::processing_overlay_kind::label:
        return stream_cell::processing_overlay_kind::label;
    }

    return stream_cell::processing_overlay_kind::point;
}

std::vector<stream_cell::processing_overlay_instance>
app_overlays_from_result(const yodau::core::processing_result& result) {
    std::vector<stream_cell::processing_overlay_instance> overlays;
    overlays.reserve(result.overlays.size());

    for (const auto& overlay_value : result.overlays) {
        stream_cell::processing_overlay_instance overlay;
        overlay.kind = app_overlay_kind(overlay_value.kind);
        overlay.label = QString::fromStdString(overlay_value.label);
        overlay.points_pct.reserve(overlay_value.points_pct.size());
        for (const auto& point_value : overlay_value.points_pct) {
            overlay.points_pct.push_back(app_point_from_core(point_value));
        }
        if (overlay_value.anchor_pct.has_value()) {
            overlay.anchor_pct = app_point_from_core(*overlay_value.anchor_pct);
        }
        overlays.push_back(std::move(overlay));
    }

    return overlays;
}

QString diagnostic_value(
    const yodau::core::processing_result& result,
    const std::vector<std::string>& keys
) {
    for (const std::string& key : keys) {
        const auto it = std::ranges::find_if(
            result.diagnostics,
            [&key](const yodau::core::processing_diagnostic& diagnostic) {
                return diagnostic.key == key;
            }
        );
        if (it != result.diagnostics.end()) {
            return QString::fromStdString(it->value);
        }
    }

    return {};
}

std::optional<double> metric_value(
    const yodau::core::processing_result& result,
    const std::vector<std::string>& names
) {
    for (const std::string& name : names) {
        const auto it = std::ranges::find_if(
            result.metrics,
            [&name](const yodau::core::processing_metric& metric) {
                return metric.name == name;
            }
        );
        if (it != result.metrics.end()) {
            return it->value;
        }
    }

    return std::nullopt;
}

QString compact_metric_number(const double value, const int precision = 0) {
    if (std::abs(value - std::round(value)) < 0.05) {
        return QString::number(static_cast<int>(std::lround(value)));
    }

    return QString::number(value, 'f', precision);
}

QString
processing_summary_from_result(const yodau::core::processing_result& result) {
    QStringList parts;

    const QString selected_algorithm
        = diagnostic_value(result, { "selected_algorithm", "algorithm" });
    if (!selected_algorithm.isEmpty()) {
        parts.push_back(selected_algorithm);
    }

    const QString background_model = diagnostic_value(
        result, { "selected_background_model", "background_model" }
    );
    const QString motion_focus
        = diagnostic_value(result, { "selected_motion_focus", "motion_focus" });
    if (!background_model.isEmpty() || !motion_focus.isEmpty()) {
        parts.push_back(
            QStringLiteral("bg %1 focus %2")
                .arg(
                    background_model.isEmpty() ? QStringLiteral("--")
                                               : background_model
                )
                .arg(
                    motion_focus.isEmpty() ? QStringLiteral("--") : motion_focus
                )
        );
    }

    if (const auto flow_count = metric_value(
            result,
            {
                "selected_sparse_flow_vector_count",
                "sparse_flow_vector_count",
            }
        )) {
        const auto flow_distance = metric_value(
            result,
            {
                "selected_flow_average_distance_pct",
                "sparse_flow_average_distance_pct",
            }
        );
        parts.push_back(QStringLiteral("flow %1 vec %2%")
                            .arg(compact_metric_number(*flow_count))
                            .arg(
                                flow_distance.has_value()
                                    ? compact_metric_number(*flow_distance, 1)
                                    : QStringLiteral("--")
                            ));
    }

    const auto track_count
        = metric_value(result, { "selected_track_count", "track_count" });
    const auto stable_track_count = metric_value(
        result, { "selected_stable_track_count", "stable_track_count" }
    );
    if (track_count.has_value() || stable_track_count.has_value()) {
        parts.push_back(QStringLiteral("tracks %1/%2")
                            .arg(
                                track_count.has_value()
                                    ? compact_metric_number(*track_count)
                                    : QStringLiteral("--")
                            )
                            .arg(
                                stable_track_count.has_value()
                                    ? compact_metric_number(*stable_track_count)
                                    : QStringLiteral("--")
                            ));
    }

    if (const auto contour_count
        = metric_value(result, { "selected_contour_count", "contour_count" })) {
        parts.push_back(QStringLiteral("contours %1")
                            .arg(compact_metric_number(*contour_count)));
    }

    const QString state = diagnostic_value(
        result,
        {
            "selected_tracking_state",
            "tracking_state",
            "selected_mask_state",
            "mask_state",
            "selected_frame_state",
            "frame_state",
        }
    );
    if (!state.isEmpty()) {
        parts.push_back(state);
    }

    return parts.join(QStringLiteral(" | "));
}

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

stream_controller::stream_controller(
    yodau::core::stream_manager* mgr, settings_panel* panel, stream_board* zone,
    yodau::observability::runtime_observer* monitor, QObject* parent
)
    : QObject(parent)
    , core_runtime(
          yodau::core::processing_runtime_options {
              .mode = yodau::core::render_mode::app_only,
              .enable_virtual_camera = false,
              .virtual_camera_device = {},
              .algorithm_id = default_app_algorithm_id().toStdString(),
          }
      )
    , stream_mgr(mgr)
    , monitor_bridge(monitor)
    , settings(panel)
    , main_zone(zone)
    , grid(zone ? zone->grid_mode() : nullptr)
    , widget_bridge(zone, panel)
    , catalog_workflow(mgr, panel, catalog_state, widget_bridge)
    , active_streams(catalog_state, route_state, widget_bridge, edit_session)
    , stream_workflow(active_streams)
    , edit_controller(edit_session, widget_bridge)
    , edit_actions(mgr, edit_session, widget_bridge, edit_controller)
    , edit_workflow(
          route_state, catalog_state, widget_bridge, edit_controller,
          edit_actions
      ) {
    qRegisterMetaType<yodau::core::event>("yodau::core::event");
    qRegisterMetaType<app_log_entry>("app_log_entry");
    qRegisterMetaType<stream_settings>("stream_settings");
    qRegisterMetaType<stream_runtime_metrics>("stream_runtime_metrics");
    qRegisterMetaType<line_profile>("line_profile");
    qRegisterMetaType<template_apply_settings>("template_apply_settings");
    qRegisterMetaType<line_edit_request>("line_edit_request");
    fps_capability = yodau::core::detect_fps_capability_profile();
    processing_pool.setMaxThreadCount(
        std::clamp(QThread::idealThreadCount(), 1, 4)
    );
    processing_pool.setExpiryTimeout(30000);
    log_buffer = new app_log_buffer(this);
    if (settings != nullptr) {
        settings->set_log_buffer(log_buffer);
        edit_controller.initialize_editor_state();
    }

    init_from_core();

    if (stream_mgr) {
        core_runtime.set_processed_frame_observer(
            [this](
                const yodau::core::stream& stream_value,
                const yodau::core::frame& frame_value,
                const yodau::core::processing_result& result
            ) {
                auto overlays
                    = stream_controller_support::app_overlays_from_result(
                        result
                    );
                const QString stream_name
                    = QString::fromStdString(stream_value.get_name());
                const QString processing_summary
                    = stream_controller_support::processing_summary_from_result(
                        result
                    );
                const int width = frame_value.width;
                const int height = frame_value.height;

                QMetaObject::invokeMethod(
                    this,
                    [this, stream_name, width, height, processing_summary,
                     overlays = std::move(overlays)]() mutable {
                        note_core_frame_observed(
                            stream_name, width, height, processing_summary
                        );
                        auto* tile = widget_bridge.tile_for_stream_name(
                            stream_name, route_state
                        );
                        if (tile != nullptr) {
                            tile->set_processing_overlays(std::move(overlays));
                        }
                    },
                    Qt::QueuedConnection
                );
            }
        );
        core_runtime.attach(*stream_mgr);
        stream_mgr->set_event_batch_sink(
            [this](const std::vector<yodau::core::event>& events) {
                QMetaObject::invokeMethod(
                    this,
                    [this, copied_events = events]() {
                        on_core_events(copied_events);
                    },
                    Qt::QueuedConnection
                );
            }
        );
    }

    if (settings && grid) {
        widget_bridge.sync_active_candidates();
        widget_bridge.sync_active_selection(
            route_state.active_stream_name(), stream_settings {}
        );
        widget_bridge.sync_visible_log_mode(settings->log_mode());
    }

    setup_settings_connections();
    if (settings != nullptr) {
        on_stream_settings_selection_changed(
            settings->current_active_stream_settings().stream_name
        );
    }
    setup_grid_connections();
    refresh_fps_policy(true);
    update_monitor_inventory();
}

stream_controller::~stream_controller() {
    shutting_down.store(true, std::memory_order_release);
    core_runtime.set_processed_frame_observer({});
    if (stream_mgr != nullptr) {
        stream_mgr->set_event_batch_sink({});
    }
    processing_pool.clear();
    processing_pool.waitForDone();
    core_runtime.detach();
}

void stream_controller::update_monitor_inventory() {
    if (!stream_mgr || monitor_bridge == nullptr) {
        return;
    }

    const int configured_streams
        = static_cast<int>(stream_mgr->stream_names().size());
    const int visible_streams
        = grid != nullptr ? static_cast<int>(grid->stream_names().size()) : 0;
    const int active_stream_count = route_state.has_active_stream() ? 1 : 0;
    const int configured_lines
        = static_cast<int>(stream_mgr->line_names().size());

    const int detected_local_sources
        = static_cast<int>(stream_mgr->detected_local_stream_names().size());

    monitor_bridge->update_inventory(
        yodau::observability::inventory_statistics {
            .configured_streams = configured_streams,
            .visible_streams = visible_streams,
            .active_streams = active_stream_count,
            .configured_lines = configured_lines,
            .detected_local_sources = detected_local_sources,
        }
    );
}

void stream_controller::update_monitor_processing_statistics(const bool force) {
    if (monitor_bridge == nullptr || !monitor_bridge->wants_event_details()) {
        return;
    }

    const auto now = stream_controller_support::steady_clock::now();
    if (!force && last_monitor_statistics_update.time_since_epoch().count() != 0
        && now - last_monitor_statistics_update
            < stream_controller_support::fps_policy_refresh_interval) {
        return;
    }
    last_monitor_statistics_update = now;

    double input_fps_total = 0.0;
    double core_fps_total = 0.0;
    double configured_core_fps_total = 0.0;
    double configured_display_fps_total = 0.0;
    int input_fps_count = 0;
    int core_fps_count = 0;
    int configured_core_fps_count = 0;
    int configured_display_fps_count = 0;
    for (auto it = runtime_metrics_by_stream.cbegin();
         it != runtime_metrics_by_stream.cend(); ++it) {
        const stream_runtime_metrics& metrics = it.value();
        if (metrics.input_fps > 0.0) {
            input_fps_total += metrics.input_fps;
            ++input_fps_count;
        }
        if (metrics.core_fps > 0.0) {
            core_fps_total += metrics.core_fps;
            ++core_fps_count;
        }
        if (metrics.effective_core_fps > 0) {
            configured_core_fps_total += metrics.effective_core_fps;
            ++configured_core_fps_count;
        }
        if (metrics.effective_display_fps > 0) {
            configured_display_fps_total += metrics.effective_display_fps;
            ++configured_display_fps_count;
        }
    }

    quint64 dropped_gui_frames = 0;
    {
        QMutexLocker lock(&pending_gui_frames_mutex);
        for (auto it = pending_gui_frames.cbegin();
             it != pending_gui_frames.cend(); ++it) {
            dropped_gui_frames += it->dropped_frames;
        }
    }

    const auto average = [](const double total, const int count) {
        return count > 0 ? total / static_cast<double>(count) : 0.0;
    };
    monitor_bridge->update_processing(
        yodau::observability::processing_statistics {
            .frame_processing_time_ms = processing_cost_ema_ms,
            .input_fps = average(input_fps_total, input_fps_count),
            .processing_fps = average(core_fps_total, core_fps_count),
            .configured_processing_fps
            = average(configured_core_fps_total, configured_core_fps_count),
            .configured_display_fps = average(
                configured_display_fps_total, configured_display_fps_count
            ),
            .dropped_gui_frames = dropped_gui_frames,
        }
    );
}

void stream_controller::init_from_core() { catalog_workflow.seed_from_core(); }

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
        yodau::data::save_line_configuration_file_atomic(
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
            = yodau::data::encode_line_configuration_json(document);
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
        const auto document = yodau::data::load_line_configuration_file(
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
        const auto document = yodau::data::decode_line_configuration_json(
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
        update_monitor_inventory();
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

void stream_controller::focus_stream(const QString& stream_name) {
    const QString normalized_name = stream_name.trimmed();
    if (normalized_name.isEmpty()) {
        return;
    }
    handle_show_stream_changed(normalized_name, true);
    set_active_stream(normalized_name);
}

void stream_controller::return_to_stream_grid() { handle_back_to_grid(); }

void stream_controller::handle_add_file(
    const QString& path, const QString& name, const bool loop
) {
    apply_catalog_result(catalog_workflow.add_stream(path, name, "file", loop));
}

void stream_controller::handle_add_local(
    const QString& source, const QString& name
) {
    apply_catalog_result(
        catalog_workflow.add_stream(source, name, "local", true)
    );
}

void stream_controller::handle_add_url(
    const QString& url, const QString& name
) {
    apply_catalog_result(catalog_workflow.add_stream(url, name, "url", true));
}

void stream_controller::handle_detect_local_sources() {
    apply_catalog_result(catalog_workflow.detect_local_sources());
}

void stream_controller::handle_show_stream_changed(
    const QString& name, const bool show
) {
    if (!grid) {
        return;
    }

    if (show) {
        stream_widget_bridge::grid_stream_binding binding;
        const auto s = stream_mgr->find_stream(name.toStdString());
        if (s) {
            binding.loop = s->is_looping();
            binding.path = QString::fromStdString(s->get_path());
            binding.type = QString::fromStdString(
                yodau::core::stream::type_name(s->get_type())
            );
        }

        if (auto* tile = widget_bridge.show_stream_in_grid(
                name, settings_for_stream(name), edit_session, binding
            )) {
            connect(
                tile, &stream_cell::frame_ready, this,
                &stream_controller::on_gui_frame, Qt::UniqueConnection
            );
        }
    } else {
        widget_bridge.hide_stream_from_grid(
            name, route_state.hide_stream(name)
        );
    }

    append_log(
        app_log_area::streams, app_log_severity::info,
        QStringLiteral("grid_visibility"),
        show ? QStringLiteral("stream shown in grid")
             : QStringLiteral("stream hidden from grid"),
        name
    );

    refresh_fps_policy(true);
    update_monitor_inventory();
    if (monitor_bridge != nullptr) {
        const std::string marker = show ? "stream_visible" : "stream_hidden";
        monitor_bridge->add_marker(marker);
    }
}

void stream_controller::handle_core_event(const QString& text) {
    append_log(
        app_log_area::active, app_log_severity::info,
        QStringLiteral("core_event"), text
    );
}

stream_settings
stream_controller::settings_for_stream(const QString& name) const {
    return catalog_state.settings_for(name);
}

QString stream_controller::algorithm_id_for_stream(const QString& name) const {
    return catalog_state.algorithm_id_for(name);
}

void stream_controller::set_active_stream(const QString& name) {
    if (!main_zone) {
        return;
    }

    apply_active_stream_result(stream_workflow.set_active_stream(name));
}

void stream_controller::on_active_stream_settings_changed(
    stream_settings settings_value
) {
    settings_value = stream_catalog_state::normalized_stream_settings(
        std::move(settings_value)
    );
    const QString stream_name = settings_value.stream_name;
    const QString algorithm_id = settings_value.algorithm_id;
    const QString preset_id = settings_value.algorithm_preset;

    apply_active_stream_result(
        stream_workflow.apply_stream_settings(std::move(settings_value))
    );

    sync_core_stream_algorithm(stream_name, algorithm_id, preset_id);
}

void stream_controller::on_stream_settings_selection_changed(
    const QString& name
) {
    if (settings == nullptr) {
        return;
    }

    settings->set_active_stream_settings(catalog_state.settings_for(name));
}

void stream_controller::on_active_stream_selected(const QString& name) {
    set_active_stream(name);
}

void stream_controller::on_active_edit_mode_changed(bool drawing_new) {
    apply_active_edit_result(edit_workflow.set_drawing_new_mode(drawing_new));
}

void stream_controller::on_active_line_profile_changed(
    line_profile profile_value
) {
    apply_active_edit_result(
        edit_workflow.apply_line_profile(std::move(profile_value))
    );
}

void stream_controller::on_active_line_save_requested(
    line_profile profile_value
) {
    apply_active_edit_result(
        edit_workflow.save_active_line(std::move(profile_value))
    );
}

void stream_controller::on_active_line_enabled_changed(
    const QString& line_name, const bool enabled
) {
    apply_active_edit_result(
        edit_workflow.set_active_line_enabled(line_name, enabled)
    );
}

void stream_controller::on_active_line_detach_requested(
    const QString& line_name
) {
    apply_active_edit_result(edit_workflow.detach_active_line(line_name));
}

void stream_controller::on_line_edit_preview_changed(
    line_edit_request request
) {
    apply_active_edit_result(
        edit_workflow.apply_line_edit_preview(std::move(request))
    );
}

void stream_controller::on_line_edit_preview_cleared() {
    apply_active_edit_result(edit_workflow.clear_line_edit_preview());
}

void stream_controller::on_line_edit_save_requested(line_edit_request request) {
    apply_active_edit_result(
        edit_workflow.save_active_line_edit(std::move(request))
    );
}

void stream_controller::on_active_template_add_requested(
    template_apply_settings settings_value
) {
    apply_active_edit_result(
        edit_workflow.apply_active_template(std::move(settings_value))
    );
}

void stream_controller::on_active_template_settings_changed(
    template_apply_settings settings_value
) {
    apply_active_edit_result(
        edit_workflow.apply_template_settings(std::move(settings_value))
    );
}

void stream_controller::on_active_line_undo_requested() {
    edit_workflow.undo_last_draft_point();
}

void stream_controller::setup_settings_connections() {
    if (!settings || !main_zone) {
        return;
    }

    connect(
        settings, &settings_panel::stream_settings_selection_changed, this,
        &stream_controller::on_stream_settings_selection_changed
    );
    connect(
        settings, &settings_panel::active_stream_settings_changed, this,
        &stream_controller::on_active_stream_settings_changed
    );
    connect(
        settings, &settings_panel::log_mode_changed, this,
        [this](const app_log_mode mode) {
            widget_bridge.sync_visible_log_mode(mode);
        }
    );

    connect(
        settings, &settings_panel::active_stream_selected, this,
        &stream_controller::on_active_stream_selected
    );

    connect(
        settings, &settings_panel::active_edit_mode_changed, this,
        &stream_controller::on_active_edit_mode_changed
    );

    connect(
        settings, &settings_panel::active_line_profile_changed, this,
        &stream_controller::on_active_line_profile_changed
    );

    connect(
        settings, &settings_panel::active_line_save_requested, this,
        &stream_controller::on_active_line_save_requested
    );

    connect(
        settings, &settings_panel::active_line_enabled_changed, this,
        &stream_controller::on_active_line_enabled_changed
    );
    connect(
        settings, &settings_panel::active_line_detach_requested, this,
        &stream_controller::on_active_line_detach_requested
    );
    connect(
        settings, &settings_panel::active_line_edit_preview_changed, this,
        &stream_controller::on_line_edit_preview_changed
    );
    connect(
        settings, &settings_panel::active_line_edit_preview_cleared, this,
        &stream_controller::on_line_edit_preview_cleared
    );
    connect(
        settings, &settings_panel::active_line_edit_save_requested, this,
        &stream_controller::on_line_edit_save_requested
    );

    connect(
        settings, &settings_panel::active_template_add_requested, this,
        &stream_controller::on_active_template_add_requested
    );

    connect(
        settings, &settings_panel::active_line_undo_requested, this,
        &stream_controller::on_active_line_undo_requested
    );

    connect(
        settings, &settings_panel::active_template_settings_changed, this,
        &stream_controller::on_active_template_settings_changed
    );
}

void stream_controller::setup_grid_connections() {
    if (!grid) {
        return;
    }

    connect(
        grid, &grid_view::stream_closed, this,
        &stream_controller::on_grid_stream_closed
    );

    connect(
        grid, &grid_view::stream_enlarge, this,
        &stream_controller::handle_enlarge_requested
    );
}

void stream_controller::on_grid_stream_closed(const QString& name) {
    if (settings) {
        settings->set_stream_checked(name, false);
    }
}

void stream_controller::handle_enlarge_requested(const QString& name) {
    const QString next_active_name
        = route_state.next_active_stream_for_enlarge(name);
    if (next_active_name == route_state.active_stream_name()) {
        return;
    }

    set_active_stream(next_active_name);
}

void stream_controller::handle_back_to_grid() { set_active_stream(QString()); }

void stream_controller::handle_thumb_activate(const QString& name) {
    handle_enlarge_requested(name);
}

void stream_controller::sync_core_stream_algorithm(
    const QString& stream_name, const QString& algorithm_id,
    const QString& preset_id
) {
    if (stream_name.isEmpty()) {
        return;
    }

    const QString normalized_algorithm_id
        = normalized_app_algorithm_id(algorithm_id);
    const QString normalized_preset_id
        = normalized_algorithm_preset_id(normalized_algorithm_id, preset_id);
    if (core_runtime.set_stream_algorithm(
            stream_name.toStdString(), normalized_algorithm_id.toStdString(),
            normalized_preset_id.toStdString()
        )) {
        return;
    }

    append_log(
        app_log_area::active, app_log_severity::error,
        QStringLiteral("stream_settings"),
        QStringLiteral("core algorithm update failed"), stream_name,
        QStringLiteral("%1/%2").arg(
            normalized_algorithm_id, normalized_preset_id
        ),
        normalized_algorithm_id
    );
}

void stream_controller::append_log_entry(app_log_entry entry) const {
    if (log_buffer != nullptr) {
        log_buffer->append(std::move(entry));
        return;
    }

    if (settings != nullptr) {
        settings->append_log(std::move(entry));
    }
}

void stream_controller::append_log_entries(
    const QVector<app_log_entry>& entries
) const {
    for (const app_log_entry& entry : entries) {
        append_log_entry(entry);
    }
}

void stream_controller::append_log(
    const app_log_area area, const app_log_severity severity,
    const QString& subsystem, const QString& message,
    const QString& stream_name, const QString& detail,
    const QString& algorithm_id, const QString& line_name,
    const QString& event_type, const QColor& line_color
) const {
    const QString resolved_algorithm_id = stream_name.isEmpty()
        ? QString()
        : (algorithm_id.isEmpty() ? algorithm_id_for_stream(stream_name)
                                  : normalized_app_algorithm_id(algorithm_id));

    app_log_entry entry {
        .timestamp = QDateTime(),
        .area = area,
        .severity = severity,
        .subsystem = subsystem,
        .stream_name = stream_name,
        .line_name = line_name,
        .algorithm_id = resolved_algorithm_id,
        .event_type = event_type,
        .message = message,
        .detail = detail,
        .line_color = line_color,
    };

    append_log_entry(std::move(entry));
}

QColor stream_controller::resolved_log_line_color(
    const QString& stream_name, const QString& line_name
) const {
    if (stream_name.trimmed().isEmpty() || line_name.trimmed().isEmpty()) {
        return {};
    }

    const auto& stream_lines = edit_session.stream_lines(stream_name);
    const int line_count = std::max(
        1,
        static_cast<int>(std::count_if(
            stream_lines.cbegin(), stream_lines.cend(),
            [](const stream_cell::line_instance& line_value) {
                return line_value.enabled;
            }
        ))
    );
    int enabled_line_index = 0;
    for (int line_index = 0; line_index < static_cast<int>(stream_lines.size());
         line_index += 1) {
        const auto& line_value = stream_lines[static_cast<size_t>(line_index)];
        if (!line_value.enabled) {
            continue;
        }
        if (line_value.template_name.trimmed() != line_name.trimmed()) {
            enabled_line_index += 1;
            continue;
        }

        const QString color_mode_id
            = normalized_line_color_mode_id(line_value.color_mode_id);
        if (color_mode_id == QStringLiteral("manual")
            && line_value.color.isValid()) {
            return line_value.color;
        }

        return auto_palette_line_color(enabled_line_index, line_count);
    }

    return {};
}

QColor stream_controller::resolved_overlay_line_color(
    const QString& stream_name, const QString& line_name,
    const QColor& fallback_color
) const {
    if (stream_name.trimmed().isEmpty() || line_name.trimmed().isEmpty()) {
        return fallback_color;
    }

    const auto& stream_lines = edit_session.stream_lines(stream_name);
    const int line_count = std::max(
        1,
        static_cast<int>(std::count_if(
            stream_lines.cbegin(), stream_lines.cend(),
            [](const stream_cell::line_instance& line_value) {
                return line_value.enabled;
            }
        ))
    );
    int enabled_line_index = 0;
    for (int line_index = 0; line_index < static_cast<int>(stream_lines.size());
         line_index += 1) {
        const auto& line_value = stream_lines[static_cast<size_t>(line_index)];
        if (!line_value.enabled) {
            continue;
        }
        if (line_value.template_name.trimmed() != line_name.trimmed()) {
            enabled_line_index += 1;
            continue;
        }

        const QString color_mode_id
            = normalized_line_color_mode_id(line_value.color_mode_id);
        if (color_mode_id == QStringLiteral("negative_auto")) {
            return fallback_color;
        }
        if (color_mode_id == QStringLiteral("manual")
            && line_value.color.isValid()) {
            return line_value.color;
        }

        return auto_palette_line_color(enabled_line_index, line_count);
    }

    return fallback_color;
}

void stream_controller::apply_active_edit_result(
    const active_edit_workflow::transition_result& result
) {
    append_log_entries(result.entries);

    if (result.refresh_fps) {
        refresh_fps_policy(true);
    }

    if (result.update_monitor_inventory) {
        update_monitor_inventory();
    }

    if (monitor_bridge != nullptr && !result.monitor_marker.isEmpty()) {
        const std::string marker = result.monitor_marker.toStdString();
        monitor_bridge->add_marker(marker);
    }
}

void stream_controller::apply_active_stream_result(
    const active_stream_workflow::transition_result& result
) {
    append_log_entries(result.entries);

    if (result.refresh_fps) {
        refresh_fps_policy(true);
    }

    if (result.update_monitor_inventory) {
        update_monitor_inventory();
    }

    if (monitor_bridge != nullptr && !result.monitor_marker.isEmpty()) {
        const std::string marker = result.monitor_marker.toStdString();
        monitor_bridge->add_marker(marker);
    }
}

void stream_controller::apply_catalog_result(
    const stream_catalog_workflow::transition_result& result
) {
    append_log_entries(result.entries);

    for (const QString& removed_stream : result.removed_streams) {
        const bool removed_active = route_state.hide_stream(removed_stream);
        edit_session.replace_stream_lines(removed_stream, {});
        processing_scale_percent_by_stream.remove(removed_stream);
        virtual_camera_path_by_stream.remove(removed_stream);
        runtime_metrics_by_stream.remove(removed_stream);
        rate_trackers_by_stream.remove(removed_stream);
        {
            QMutexLocker lock(&pending_gui_frames_mutex);
            pending_gui_frames.remove(removed_stream);
        }
        if (removed_active) {
            widget_bridge.sync_active_selection(QString(), stream_settings {});
        }
    }

    if (!result.added_stream.isEmpty()) {
        if (settings != nullptr) {
            settings->set_stream_checked(result.added_stream, true);
        }
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
        // On a phone, opening the new stream immediately avoids a round trip
        // through the compact Streams page. Desktop keeps its established
        // multi-stream workflow and selection state.
        focus_stream(result.added_stream);
#endif
    }

    if (result.refresh_fps || !result.removed_streams.isEmpty()) {
        refresh_fps_policy(true);
    }

    if (result.update_monitor_inventory) {
        update_monitor_inventory();
    }

    if (monitor_bridge != nullptr && !result.monitor_marker.isEmpty()) {
        const std::string marker = result.monitor_marker.toStdString();
        monitor_bridge->add_marker(marker);
    }
}

void stream_controller::refresh_fps_policy(const bool force) {
    if (!stream_mgr || !grid) {
        return;
    }

    const auto now = stream_controller_support::steady_clock::now();

    if (!force && last_fps_policy_refresh.time_since_epoch().count() != 0) {
        const auto elapsed
            = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_fps_policy_refresh
            );
        if (elapsed < stream_controller_support::fps_policy_refresh_interval) {
            return;
        }
    }

    last_fps_policy_refresh = now;

    const yodau::core::fps_mode mode = core_runtime.processing_enabled()
        ? yodau::core::fps_mode::playback_and_processing
        : yodau::core::fps_mode::playback_only;

    const int configured_stream_count
        = static_cast<int>(stream_mgr->stream_names().size());
    const int configured_line_count
        = static_cast<int>(stream_mgr->line_names().size());
    const int visible_stream_count
        = static_cast<int>(grid->stream_names().size())
        + (route_state.has_active_stream() ? 1 : 0);
    const int active_stream_count = route_state.has_active_stream() ? 1 : 0;
    const int cell_count = grid_cell_count();
    const int motion_count = feedback_state.recent_motion_count();
    const double load_ratio = current_device_load_ratio();

    const yodau::core::fps_runtime_factors default_factors {
        .mode = mode,
        .role = yodau::core::fps_stream_role::grid,
        .configured_stream_count = configured_stream_count,
        .visible_stream_count = visible_stream_count,
        .active_stream_count = active_stream_count,
        .configured_line_count = configured_line_count,
        .stream_line_count = 0,
        .grid_cell_count = cell_count,
        .recent_motion_count = motion_count,
        .device_load_ratio = load_ratio,
    };

    if (mode == yodau::core::fps_mode::playback_and_processing) {
        const auto default_profile = yodau::core::recommend_fps_profile(
            fps_capability, default_factors
        );
        stream_mgr->set_analysis_interval_ms(
            default_profile.analysis_interval_ms
        );
    }

    for (const auto& stream_name : stream_mgr->stream_names()) {
        stream_mgr->clear_stream_analysis_interval_ms(stream_name);
    }

    processing_scale_percent_by_stream.clear();

    for (const auto& name : grid->stream_names()) {
        const stream_settings settings_value = settings_for_stream(name);
        yodau::core::fps_stream_profile profile;

        if (settings_value.manual_processing_policy_enabled) {
            profile.repaint_interval_ms
                = interval_ms_for_fps(settings_value.manual_display_fps);
            profile.analysis_interval_ms
                = mode == yodau::core::fps_mode::playback_and_processing
                ? interval_ms_for_fps(settings_value.manual_core_fps)
                : 0;
            profile.processing_scale_percent = 100;
        } else {
            const yodau::core::fps_runtime_factors factors {
                .mode = mode,
                .role = yodau::core::fps_stream_role::grid,
                .configured_stream_count = configured_stream_count,
                .visible_stream_count = visible_stream_count,
                .active_stream_count = active_stream_count,
                .configured_line_count = configured_line_count,
                .stream_line_count = line_count_for_stream(name),
                .grid_cell_count = cell_count,
                .recent_motion_count = motion_count,
                .device_load_ratio = load_ratio,
            };
            profile
                = yodau::core::recommend_fps_profile(fps_capability, factors);
        }

        if (auto* tile = grid->peek_stream_cell(name)) {
            tile->set_repaint_interval_ms(profile.repaint_interval_ms);
        }

        processing_scale_percent_by_stream.insert(
            name, profile.processing_scale_percent
        );

        stream_runtime_metrics metrics = runtime_metrics_by_stream.value(name);
        metrics.manual_policy_active
            = settings_value.manual_processing_policy_enabled;
        metrics.effective_display_fps
            = fps_for_interval_ms(profile.repaint_interval_ms);
        metrics.effective_core_fps
            = mode == yodau::core::fps_mode::playback_and_processing
            ? fps_for_interval_ms(profile.analysis_interval_ms)
            : 0;
        if (mode != yodau::core::fps_mode::playback_and_processing) {
            metrics.core_fps = 0.0;
        }
        if (settings_value.manual_processing_policy_enabled) {
            metrics.effective_processing_pixels
                = settings_value.manual_processing_pixels;
        } else if (metrics.input_width > 0 && metrics.input_height > 0) {
            const double scale
                = static_cast<double>(profile.processing_scale_percent) / 100.0;
            metrics.effective_processing_pixels = static_cast<int>(std::lround(
                static_cast<double>(metrics.input_width * metrics.input_height)
                * scale * scale
            ));
        }
        runtime_metrics_by_stream.insert(name, metrics);

        if (mode == yodau::core::fps_mode::playback_and_processing) {
            stream_mgr->set_stream_analysis_interval_ms(
                name.toStdString(), profile.analysis_interval_ms
            );
        }
    }

    const QString active_name = route_state.active_stream_name();
    if (!active_name.isEmpty()) {
        const stream_settings settings_value = settings_for_stream(active_name);
        yodau::core::fps_stream_profile profile;

        if (settings_value.manual_processing_policy_enabled) {
            profile.repaint_interval_ms
                = interval_ms_for_fps(settings_value.manual_display_fps);
            profile.analysis_interval_ms
                = mode == yodau::core::fps_mode::playback_and_processing
                ? interval_ms_for_fps(settings_value.manual_core_fps)
                : 0;
            profile.processing_scale_percent = 100;
        } else {
            const yodau::core::fps_runtime_factors factors {
                .mode = mode,
                .role = yodau::core::fps_stream_role::active,
                .configured_stream_count = configured_stream_count,
                .visible_stream_count = visible_stream_count,
                .active_stream_count = active_stream_count,
                .configured_line_count = configured_line_count,
                .stream_line_count = line_count_for_stream(active_name),
                .grid_cell_count = cell_count,
                .recent_motion_count = motion_count,
                .device_load_ratio = load_ratio,
            };
            profile
                = yodau::core::recommend_fps_profile(fps_capability, factors);
        }

        if (main_zone != nullptr) {
            if (auto* cell = widget_bridge.active_cell()) {
                cell->set_repaint_interval_ms(profile.repaint_interval_ms);
            }
        }

        processing_scale_percent_by_stream.insert(
            active_name, profile.processing_scale_percent
        );

        stream_runtime_metrics metrics
            = runtime_metrics_by_stream.value(active_name);
        metrics.manual_policy_active
            = settings_value.manual_processing_policy_enabled;
        metrics.effective_display_fps
            = fps_for_interval_ms(profile.repaint_interval_ms);
        metrics.effective_core_fps
            = mode == yodau::core::fps_mode::playback_and_processing
            ? fps_for_interval_ms(profile.analysis_interval_ms)
            : 0;
        if (mode != yodau::core::fps_mode::playback_and_processing) {
            metrics.core_fps = 0.0;
        }
        if (settings_value.manual_processing_policy_enabled) {
            metrics.effective_processing_pixels
                = settings_value.manual_processing_pixels;
        } else if (metrics.input_width > 0 && metrics.input_height > 0) {
            const double scale
                = static_cast<double>(profile.processing_scale_percent) / 100.0;
            metrics.effective_processing_pixels = static_cast<int>(std::lround(
                static_cast<double>(metrics.input_width * metrics.input_height)
                * scale * scale
            ));
        }
        runtime_metrics_by_stream.insert(active_name, metrics);

        if (mode == yodau::core::fps_mode::playback_and_processing) {
            stream_mgr->set_stream_analysis_interval_ms(
                active_name.toStdString(), profile.analysis_interval_ms
            );
        }
    }

    sync_visible_runtime_metrics();
}

int stream_controller::grid_cell_count() const {
    if (!grid) {
        return 0;
    }

    const int layout_cells = grid->layout_cell_count();
    if (layout_cells > 0) {
        return layout_cells;
    }

    return static_cast<int>(grid->stream_names().size());
}

int stream_controller::line_count_for_stream(const QString& stream_name) const {
    if (!stream_mgr || stream_name.isEmpty()) {
        return 0;
    }

    const auto stream_ptr = stream_mgr->find_stream(stream_name.toStdString());
    if (!stream_ptr) {
        return 0;
    }

    return static_cast<int>(stream_ptr->line_names().size());
}

double stream_controller::current_device_load_ratio() const {
    return std::clamp(processing_cost_ema_ms / 20.0, 0.0, 2.5);
}

void stream_controller::note_input_frame_observed(
    const QString& stream_name, const int width, const int height
) {
    if (stream_name.isEmpty()) {
        return;
    }

    auto& tracker = rate_trackers_by_stream[stream_name];
    const auto now = stream_controller_support::steady_clock::now();
    stream_runtime_metrics metrics
        = runtime_metrics_by_stream.value(stream_name);
    metrics.input_fps
        = update_fps_ema(tracker.last_input_frame, tracker.input_fps_ema, now);
    metrics.input_width = width;
    metrics.input_height = height;
    runtime_metrics_by_stream.insert(stream_name, metrics);
    sync_runtime_metrics_for_stream(stream_name);
    update_monitor_processing_statistics(false);
}

void stream_controller::note_core_frame_observed(
    const QString& stream_name, const int width, const int height,
    const QString& processing_summary
) {
    if (stream_name.isEmpty()) {
        return;
    }

    auto& tracker = rate_trackers_by_stream[stream_name];
    const auto now = stream_controller_support::steady_clock::now();
    stream_runtime_metrics metrics
        = runtime_metrics_by_stream.value(stream_name);
    metrics.core_fps
        = update_fps_ema(tracker.last_core_frame, tracker.core_fps_ema, now);
    metrics.processed_width = width;
    metrics.processed_height = height;
    metrics.effective_processing_pixels = width > 0 && height > 0
        ? width * height
        : metrics.effective_processing_pixels;
    if (!processing_summary.isNull()) {
        metrics.processing_summary = processing_summary;
    }
    runtime_metrics_by_stream.insert(stream_name, metrics);
    sync_runtime_metrics_for_stream(stream_name);
    update_monitor_processing_statistics(false);
}

void stream_controller::sync_runtime_metrics_for_stream(
    const QString& stream_name
) {
    if (stream_name.isEmpty()) {
        return;
    }

    if (auto* tile
        = widget_bridge.tile_for_stream_name(stream_name, route_state)) {
        tile->set_runtime_metrics(runtime_metrics_by_stream.value(stream_name));
    }
}

void stream_controller::sync_visible_runtime_metrics() {
    if (grid != nullptr) {
        for (const QString& name : grid->stream_names()) {
            sync_runtime_metrics_for_stream(name);
        }
    }

    const QString active_name = route_state.active_stream_name();
    if (!active_name.isEmpty()) {
        sync_runtime_metrics_for_stream(active_name);
    }
}

int stream_controller::interval_ms_for_fps(const int fps) {
    return fps <= 0 ? 0
                    : std::max(1, static_cast<int>(std::lround(1000.0 / fps)));
}

int stream_controller::fps_for_interval_ms(const int interval_ms) {
    return interval_ms <= 0
        ? 0
        : std::max(1, static_cast<int>(std::lround(1000.0 / interval_ms)));
}

double stream_controller::update_fps_ema(
    std::chrono::steady_clock::time_point& last_sample, double& ema_fps,
    const std::chrono::steady_clock::time_point now
) {
    if (last_sample.time_since_epoch().count() == 0) {
        last_sample = now;
        return ema_fps;
    }

    const double elapsed_ms
        = std::chrono::duration<double, std::milli>(now - last_sample).count();
    last_sample = now;
    if (elapsed_ms <= 1.0) {
        return ema_fps;
    }

    const double sample_fps = 1000.0 / elapsed_ms;
    constexpr double smoothing = 0.18;
    ema_fps = ema_fps <= 0.0
        ? sample_fps
        : ema_fps * (1.0 - smoothing) + sample_fps * smoothing;
    return ema_fps;
}

void stream_controller::note_processing_cost_sample(const double elapsed_ms) {
    if (elapsed_ms <= 0.0) {
        return;
    }

    constexpr double smoothing = 0.18;

    if (processing_cost_ema_ms <= 0.0) {
        processing_cost_ema_ms = elapsed_ms;
        update_monitor_processing_statistics(false);
        return;
    }

    processing_cost_ema_ms
        = processing_cost_ema_ms * (1.0 - smoothing) + elapsed_ms * smoothing;
    update_monitor_processing_statistics(false);
}

QSize stream_controller::processing_image_size(
    const QString& stream_name, const QImage& image
) const {
    if (image.isNull()) {
        return {};
    }

    const stream_settings settings_value = settings_for_stream(stream_name);
    if (settings_value.manual_processing_policy_enabled) {
        const qint64 current_pixels
            = static_cast<qint64>(image.width()) * image.height();
        const qint64 target_pixels
            = std::max(1, settings_value.manual_processing_pixels);
        if (target_pixels >= current_pixels) {
            return image.size();
        }

        const double scale = std::sqrt(
            static_cast<double>(target_pixels)
            / static_cast<double>(current_pixels)
        );
        const QSize scaled_size(
            std::max(1, static_cast<int>(std::lround(image.width() * scale))),
            std::max(1, static_cast<int>(std::lround(image.height() * scale)))
        );

        return scaled_size;
    }

    const int scale_percent = std::clamp(
        processing_scale_percent_by_stream.value(stream_name, 100), 1, 100
    );
    if (scale_percent >= 100) {
        return image.size();
    }

    const QSize scaled_size(
        std::max(1, image.width() * scale_percent / 100),
        std::max(1, image.height() * scale_percent / 100)
    );

    return scaled_size;
}

void stream_controller::on_core_events(
    const std::vector<yodau::core::event>& evs
) {
    if (monitor_bridge != nullptr && monitor_bridge->wants_event_details()) {
        std::vector<yodau::observability::runtime_event_view> observed;
        observed.reserve(evs.size());
        for (const auto& event : evs) {
            yodau::observability::runtime_event_kind kind
                = yodau::observability::runtime_event_kind::info;
            switch (event.kind) {
            case yodau::core::event_kind::motion:
                kind = yodau::observability::runtime_event_kind::motion;
                break;
            case yodau::core::event_kind::tripwire:
                kind = yodau::observability::runtime_event_kind::tripwire;
                break;
            case yodau::core::event_kind::roi:
                kind = yodau::observability::runtime_event_kind::roi;
                break;
            case yodau::core::event_kind::info:
                break;
            }
            observed.push_back(
                yodau::observability::runtime_event_view {
                    .kind = kind,
                    .timestamp = event.ts,
                    .stream_name = event.stream_name,
                    .line_name = event.line_name,
                    .message = event.message,
                    .position_x_pct = event.pos_pct.has_value()
                        ? std::optional<double>(event.pos_pct->x)
                        : std::nullopt,
                    .position_y_pct = event.pos_pct.has_value()
                        ? std::optional<double>(event.pos_pct->y)
                        : std::nullopt,
                }
            );
        }
        monitor_bridge->record_events(observed);
    }
    for (const auto& e : evs) {
        on_core_event(e);
    }
}

yodau::core::frame stream_controller::frame_from_image(const QImage& image) {
    QImage img = image;

    if (img.format() != QImage::Format_RGB888) {
        img = img.convertToFormat(QImage::Format_RGB888);
    }

    yodau::core::frame f;
    f.width = img.width();
    f.height = img.height();
    f.stride = static_cast<int>(img.bytesPerLine());
    f.format = yodau::core::pixel_format::rgb24;
    f.ts = std::chrono::steady_clock::now();

    const auto* ptr = img.constBits();
    const int bytes = static_cast<int>(img.sizeInBytes());
    if (ptr && bytes > 0) {
        f.data.assign(ptr, ptr + bytes);
    }

    return f;
}

void stream_controller::on_gui_frame(
    const QString& stream_name, const QImage& image
) {
    if (!stream_mgr) {
        return;
    }

    note_input_frame_observed(stream_name, image.width(), image.height());

    if (!core_runtime.processing_enabled()) {
        return;
    }

    if (image.isNull()) {
        return;
    }

    const QSize target_size = processing_image_size(stream_name, image);
    stream_runtime_metrics metrics
        = runtime_metrics_by_stream.value(stream_name);
    metrics.processed_width = target_size.width();
    metrics.processed_height = target_size.height();
    metrics.effective_processing_pixels
        = target_size.width() * target_size.height();
    runtime_metrics_by_stream.insert(stream_name, metrics);
    sync_runtime_metrics_for_stream(stream_name);

    bool schedule_worker = false;
    int worker_delay_ms = 0;
    {
        QMutexLocker lock(&pending_gui_frames_mutex);
        pending_gui_frame& pending = pending_gui_frames[stream_name];
        if (!pending.latest_image.isNull()) {
            ++pending.dropped_frames;
        }
        pending.latest_image = image;
        pending.processing_size = target_size;
        pending.minimum_interval_ms = interval_ms_for_fps(
            metrics.effective_core_fps > 0 ? metrics.effective_core_fps
                                           : default_manual_core_fps()
        );

        const auto now = stream_controller_support::steady_clock::now();
        if (!pending.worker_scheduled) {
            pending.worker_scheduled = true;
            schedule_worker = true;
            if (pending.last_scheduled.time_since_epoch().count() != 0) {
                const auto ready_at = pending.last_scheduled
                    + std::chrono::milliseconds(pending.minimum_interval_ms);
                if (ready_at > now) {
                    worker_delay_ms = static_cast<int>(
                        std::chrono::ceil<std::chrono::milliseconds>(
                            ready_at - now
                        )
                            .count()
                    );
                }
            }
        }
    }

    if (schedule_worker) {
        schedule_gui_frame_worker(stream_name, worker_delay_ms);
    }
}

void stream_controller::schedule_gui_frame_worker(
    const QString& stream_name, const int delay_ms
) {
    QMetaObject::invokeMethod(
        this,
        [this, stream_name, delay_ms]() {
            QTimer::singleShot(
                std::max(0, delay_ms), this, [this, stream_name]() {
                    bool launch_worker = false;
                    {
                        QMutexLocker lock(&pending_gui_frames_mutex);
                        auto it = pending_gui_frames.find(stream_name);
                        if (it == pending_gui_frames.end()
                            || shutting_down.load(std::memory_order_acquire)
                            || it->latest_image.isNull()) {
                            if (it != pending_gui_frames.end()) {
                                it->worker_scheduled = false;
                            }
                            return;
                        }
                        it->last_scheduled
                            = stream_controller_support::steady_clock::now();
                        launch_worker = true;
                    }

                    if (launch_worker) {
                        processing_pool.start([this, stream_name]() {
                            drain_latest_gui_frames(stream_name);
                        });
                    }
                }
            );
        },
        Qt::QueuedConnection
    );
}

void stream_controller::drain_latest_gui_frames(const QString& stream_name) {
    QImage image;
    QSize target_size;
    {
        QMutexLocker lock(&pending_gui_frames_mutex);
        auto it = pending_gui_frames.find(stream_name);
        if (it == pending_gui_frames.end()
            || shutting_down.load(std::memory_order_acquire)) {
            if (it != pending_gui_frames.end()) {
                it->worker_scheduled = false;
            }
            return;
        }
        image = std::move(it->latest_image);
        it->latest_image = {};
        target_size = it->processing_size;
    }

    const auto started = stream_controller_support::steady_clock::now();
    if (!image.isNull() && target_size.isValid()
        && image.size() != target_size) {
        image = image.scaled(
            target_size, Qt::IgnoreAspectRatio, Qt::FastTransformation
        );
    }
    if (!image.isNull() && !shutting_down.load(std::memory_order_acquire)
        && stream_mgr != nullptr) {
        auto frame_value = frame_from_image(image);
        stream_mgr->push_frame(
            stream_name.toStdString(), std::move(frame_value)
        );
    }
    const auto finished = stream_controller_support::steady_clock::now();
    const double elapsed_ms
        = std::chrono::duration<double, std::milli>(finished - started).count();

    bool schedule_next = false;
    int next_delay_ms = 0;
    {
        QMutexLocker lock(&pending_gui_frames_mutex);
        auto it = pending_gui_frames.find(stream_name);
        if (it != pending_gui_frames.end()) {
            if (!shutting_down.load(std::memory_order_acquire)
                && !it->latest_image.isNull()) {
                const auto now = stream_controller_support::steady_clock::now();
                const auto ready_at = it->last_scheduled
                    + std::chrono::milliseconds(it->minimum_interval_ms);
                if (ready_at > now) {
                    next_delay_ms = static_cast<int>(
                        std::chrono::ceil<std::chrono::milliseconds>(
                            ready_at - now
                        )
                            .count()
                    );
                }
                schedule_next = true;
            } else {
                it->worker_scheduled = false;
            }
        }
    }

    if (schedule_next) {
        schedule_gui_frame_worker(stream_name, next_delay_ms);
    }

    if (!shutting_down.load(std::memory_order_acquire)) {
        QMetaObject::invokeMethod(
            this,
            [this, elapsed_ms]() {
                note_processing_cost_sample(elapsed_ms);
                refresh_fps_policy(false);
            },
            Qt::QueuedConnection
        );
    }
}

void stream_controller::on_core_frame_processed(
    const QString& stream_name, const int width, const int height
) {
    note_core_frame_observed(stream_name, width, height);
}

void stream_controller::on_core_event(const yodau::core::event& e) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(
            this,
            [this, event_value = e]() { on_core_event_queued(event_value); },
            Qt::QueuedConnection
        );
        return;
    }

    on_core_event_queued(e);
}

void stream_controller::on_core_event_queued(
    const yodau::core::event& event_value
) {
    const processing_feedback_state::processed_event feedback
        = feedback_state.consume_event(event_value);

    append_log(
        app_log_area::active, feedback.log_severity,
        QStringLiteral("core_event"), feedback.log_message,
        feedback.stream_name, feedback.log_detail, QString(),
        feedback.line_name, feedback.kind_text,
        resolved_log_line_color(feedback.stream_name, feedback.line_name)
    );

    if (feedback.motion_activity_changed) {
        refresh_fps_policy(false);
    }

    auto* tile
        = widget_bridge.tile_for_stream_name(feedback.stream_name, route_state);
    if (!tile) {
        return;
    }

    if (!feedback.overlay_position_pct.has_value()
        || !feedback.allow_gui_overlay) {
        return;
    }

    if (!feedback.line_name.isEmpty()) {
        const processing_feedback_state::tripwire_visual_feedback visual
            = feedback.tripwire_visual.value_or(
                processing_feedback_state::tripwire_visual_feedback {}
            );
        tile->highlight_line_at(
            feedback.line_name, *feedback.overlay_position_pct, visual.strength,
            visual.direction, visual.speed
        );
    }

    tile->add_event(
        *feedback.overlay_position_pct,
        resolved_overlay_line_color(
            feedback.stream_name, feedback.line_name, feedback.overlay_color
        )
    );
}
