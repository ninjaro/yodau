#include "shell/stream_controller.hpp"
#include "streams/stream.hpp"
#include "widgets/settings_panel.hpp"

#include <QMetaObject>
#include <QStringList>
#include <QThread>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

#include "streams/event.hpp"
#include "streams/frame.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/stream_board.hpp"
#include "widgets/stream_cell.hpp"

Q_DECLARE_METATYPE(yodau::core::event)

namespace stream_controller_support {

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

} // namespace stream_controller_support

stream_controller::stream_controller(
    yodau::core::stream_manager* mgr, settings_panel* panel, stream_board* zone,
    QObject* parent
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

void stream_controller::init_from_core() { catalog_workflow.seed_from_core(); }

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

void stream_controller::apply_active_edit_result(
    const active_edit_workflow::transition_result& result
) {
    append_log_entries(result.entries);

    if (result.refresh_fps) {
        refresh_fps_policy(true);
    }
}

void stream_controller::apply_active_stream_result(
    const active_stream_workflow::transition_result& result
) {
    append_log_entries(result.entries);

    if (result.refresh_fps) {
        refresh_fps_policy(true);
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
}
