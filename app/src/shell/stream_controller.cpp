#include "shell/stream_controller.hpp"
#include "geometry/geometry.hpp"
#include "monitor/runtime_bridge.hpp"
#include "streams/stream.hpp"
#include "widgets/settings_panel.hpp"

#include <QCameraDevice>
#include <QColor>
#include <QDateTime>
#include <QImage>
#include <QMetaObject>
#include <QMetaType>
#include <QThread>
#include <QtGlobal>
#include <algorithm>
#include <chrono>
#include <functional>
#include <utility>

#include "streams/event.hpp"
#include "streams/frame.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/stream_board.hpp"
#include "widgets/stream_cell.hpp"

Q_DECLARE_METATYPE(yodau::backend::event)

namespace stream_controller_support {

using steady_clock = std::chrono::steady_clock;

constexpr auto fps_policy_refresh_interval = std::chrono::milliseconds(250);

} // namespace stream_controller_support

stream_controller::stream_controller(
    yodau::backend::stream_manager* mgr, settings_panel* panel,
    stream_board* zone, yodau::monitor::runtime_bridge* monitor, QObject* parent
)
    : QObject(parent)
    , backend_runtime(
          yodau::backend::processing_runtime_options {
              .mode = yodau::backend::render_mode::frontend_only,
              .enable_virtual_camera = false,
              .algorithm_id = default_frontend_algorithm_id().toStdString(),
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
    qRegisterMetaType<yodau::backend::event>("yodau::backend::event");
    qRegisterMetaType<frontend_log_entry>("frontend_log_entry");
    qRegisterMetaType<stream_settings>("stream_settings");
    qRegisterMetaType<line_profile>("line_profile");
    qRegisterMetaType<template_apply_settings>("template_apply_settings");
    fps_capability = yodau::backend::detect_fps_capability_profile();
    log_buffer = new frontend_log_buffer(this);
    if (settings != nullptr) {
        settings->set_log_buffer(log_buffer);
        edit_controller.initialize_editor_state();
    }

    init_from_backend();

    if (stream_mgr) {
        backend_runtime.attach(*stream_mgr);
        stream_mgr->set_event_batch_sink(
            std::bind_front(&stream_controller::on_backend_events, this)
        );
    }

    if (settings && grid) {
        widget_bridge.sync_active_candidates();
        widget_bridge.sync_active_selection(
            route_state.active_stream_name(), stream_settings {}
        );
    }

    setup_settings_connections();
    setup_grid_connections();
    refresh_fps_policy(true);
    update_monitor_inventory();
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

    int detected_local_sources = 0;
    for (const std::string& name : stream_mgr->stream_names()) {
        if (QString::fromStdString(name).startsWith(QStringLiteral("video"))) {
            ++detected_local_sources;
        }
    }

    monitor_bridge->set_inventory(
        configured_streams, visible_streams, active_stream_count, configured_lines,
        detected_local_sources
    );
}

void stream_controller::init_from_backend() {
    catalog_workflow.seed_from_backend();
}

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
                yodau::backend::stream::type_name(s->get_type())
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
        widget_bridge.hide_stream_from_grid(name, route_state.hide_stream(name));
    }

    append_log(
        frontend_log_area::streams, frontend_log_severity::info,
        QStringLiteral("grid_visibility"),
        show ? QStringLiteral("stream shown in grid")
             : QStringLiteral("stream hidden from grid"),
        name
    );

    emit monitor_stream_visibility_changed(name, show);
    refresh_fps_policy(true);
    update_monitor_inventory();
    if (monitor_bridge != nullptr) {
        monitor_bridge->add_marker(
            show ? QStringLiteral("stream_visible")
                 : QStringLiteral("stream_hidden")
        );
    }
}

void stream_controller::handle_backend_event(const QString& text) {
    append_log(
        frontend_log_area::active, frontend_log_severity::info,
        QStringLiteral("backend_event"), text
    );
}

stream_settings stream_controller::settings_for_stream(const QString& name) const {
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

    apply_active_stream_result(
        stream_workflow.apply_stream_settings(std::move(settings_value))
    );

    sync_backend_stream_algorithm(stream_name, algorithm_id);
}

void stream_controller::on_active_edit_mode_changed(bool drawing_new) {
    apply_active_edit_result(edit_workflow.set_drawing_new_mode(drawing_new));
}

void stream_controller::on_active_line_profile_changed(line_profile profile_value) {
    apply_active_edit_result(
        edit_workflow.apply_line_profile(std::move(profile_value))
    );
}

void stream_controller::on_active_line_save_requested(line_profile profile_value) {
    apply_active_edit_result(
        edit_workflow.save_active_line(std::move(profile_value))
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
        settings, &settings_panel::active_stream_settings_changed, this,
        &stream_controller::on_active_stream_settings_changed
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

void stream_controller::handle_back_to_grid() {
    set_active_stream(QString());
}

void stream_controller::handle_thumb_activate(const QString& name) {
    handle_enlarge_requested(name);
}

void stream_controller::sync_backend_stream_algorithm(
    const QString& stream_name, const QString& algorithm_id
) {
    if (stream_name.isEmpty()) {
        return;
    }

    const QString normalized_algorithm_id
        = normalized_frontend_algorithm_id(algorithm_id);
    if (backend_runtime.set_stream_algorithm(
            stream_name.toStdString(), normalized_algorithm_id.toStdString()
        )) {
        return;
    }

    append_log(
        frontend_log_area::active, frontend_log_severity::error,
        QStringLiteral("stream_settings"),
        QStringLiteral("backend algorithm update failed"), stream_name,
        normalized_algorithm_id, normalized_algorithm_id
    );
}

void stream_controller::append_log_entry(frontend_log_entry entry) const {
    if (log_buffer != nullptr) {
        log_buffer->append(std::move(entry));
        return;
    }

    if (settings != nullptr) {
        settings->append_log(std::move(entry));
    }
}

void stream_controller::append_log_entries(
    const QVector<frontend_log_entry>& entries
) const {
    for (const frontend_log_entry& entry : entries) {
        append_log_entry(entry);
    }
}

void stream_controller::append_log(
    const frontend_log_area area, const frontend_log_severity severity,
    const QString& subsystem, const QString& message,
    const QString& stream_name, const QString& detail,
    const QString& algorithm_id
) const {
    const QString resolved_algorithm_id = stream_name.isEmpty()
        ? QString()
        : (algorithm_id.isEmpty() ? algorithm_id_for_stream(stream_name)
                                  : normalized_frontend_algorithm_id(algorithm_id));

    frontend_log_entry entry {
        .timestamp = QDateTime(),
        .area = area,
        .severity = severity,
        .subsystem = subsystem,
        .stream_name = stream_name,
        .algorithm_id = resolved_algorithm_id,
        .message = message,
        .detail = detail,
    };

    append_log_entry(std::move(entry));
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
        monitor_bridge->add_marker(result.monitor_marker);
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
        monitor_bridge->add_marker(result.monitor_marker);
    }
}

void stream_controller::apply_catalog_result(
    const stream_catalog_workflow::transition_result& result
) {
    append_log_entries(result.entries);

    if (result.refresh_fps) {
        refresh_fps_policy(true);
    }

    if (result.update_monitor_inventory) {
        update_monitor_inventory();
    }

    if (monitor_bridge != nullptr && !result.monitor_marker.isEmpty()) {
        monitor_bridge->add_marker(result.monitor_marker);
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

    const yodau::backend::fps_mode mode = backend_runtime.processing_enabled()
        ? yodau::backend::fps_mode::playback_and_processing
        : yodau::backend::fps_mode::playback_only;

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

    const yodau::backend::fps_runtime_factors default_factors {
        .mode = mode,
        .role = yodau::backend::fps_stream_role::grid,
        .configured_stream_count = configured_stream_count,
        .visible_stream_count = visible_stream_count,
        .active_stream_count = active_stream_count,
        .configured_line_count = configured_line_count,
        .stream_line_count = 0,
        .grid_cell_count = cell_count,
        .recent_motion_count = motion_count,
        .device_load_ratio = load_ratio,
    };

    if (mode == yodau::backend::fps_mode::playback_and_processing) {
        const auto default_profile = yodau::backend::recommend_fps_profile(
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
        const yodau::backend::fps_runtime_factors factors {
            .mode = mode,
            .role = yodau::backend::fps_stream_role::grid,
            .configured_stream_count = configured_stream_count,
            .visible_stream_count = visible_stream_count,
            .active_stream_count = active_stream_count,
            .configured_line_count = configured_line_count,
            .stream_line_count = line_count_for_stream(name),
            .grid_cell_count = cell_count,
            .recent_motion_count = motion_count,
            .device_load_ratio = load_ratio,
        };
        const auto profile
            = yodau::backend::recommend_fps_profile(fps_capability, factors);

        if (auto* tile = grid->peek_stream_cell(name)) {
            tile->set_repaint_interval_ms(profile.repaint_interval_ms);
        }

        processing_scale_percent_by_stream.insert(
            name, profile.processing_scale_percent
        );

        if (mode == yodau::backend::fps_mode::playback_and_processing) {
            stream_mgr->set_stream_analysis_interval_ms(
                name.toStdString(), profile.analysis_interval_ms
            );
        }
    }

    const QString active_name = route_state.active_stream_name();
    if (!active_name.isEmpty()) {
        const yodau::backend::fps_runtime_factors factors {
            .mode = mode,
            .role = yodau::backend::fps_stream_role::active,
            .configured_stream_count = configured_stream_count,
            .visible_stream_count = visible_stream_count,
            .active_stream_count = active_stream_count,
            .configured_line_count = configured_line_count,
            .stream_line_count = line_count_for_stream(active_name),
            .grid_cell_count = cell_count,
            .recent_motion_count = motion_count,
            .device_load_ratio = load_ratio,
        };
        const auto profile
            = yodau::backend::recommend_fps_profile(fps_capability, factors);

        if (main_zone != nullptr) {
            if (auto* cell = widget_bridge.active_cell()) {
                cell->set_repaint_interval_ms(profile.repaint_interval_ms);
            }
        }

        processing_scale_percent_by_stream.insert(
            active_name, profile.processing_scale_percent
        );

        if (mode == yodau::backend::fps_mode::playback_and_processing) {
            stream_mgr->set_stream_analysis_interval_ms(
                active_name.toStdString(), profile.analysis_interval_ms
            );
        }
    }
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

void stream_controller::note_processing_cost_sample(const double elapsed_ms) {
    if (elapsed_ms <= 0.0) {
        return;
    }

    constexpr double smoothing = 0.18;

    if (processing_cost_ema_ms <= 0.0) {
        processing_cost_ema_ms = elapsed_ms;
        return;
    }

    processing_cost_ema_ms
        = processing_cost_ema_ms * (1.0 - smoothing) + elapsed_ms * smoothing;
}

QImage stream_controller::scaled_processing_image(
    const QString& stream_name, const QImage& image
) const {
    if (image.isNull()) {
        return image;
    }

    const int scale_percent = std::clamp(
        processing_scale_percent_by_stream.value(stream_name, 100), 1, 100
    );
    if (scale_percent >= 100) {
        return image;
    }

    const QSize scaled_size(
        std::max(1, image.width() * scale_percent / 100),
        std::max(1, image.height() * scale_percent / 100)
    );

    return image.scaled(
        scaled_size, Qt::IgnoreAspectRatio, Qt::FastTransformation
    );
}

void stream_controller::on_backend_events(
    const std::vector<yodau::backend::event>& evs
) {
    if (monitor_bridge != nullptr) {
        monitor_bridge->record_event_batch(evs);
    }
    for (const auto& e : evs) {
        on_backend_event(e);
    }
}

yodau::backend::frame
stream_controller::frame_from_image(const QImage& image) const {
    QImage img = image;

    if (img.format() != QImage::Format_RGB888) {
        img = img.convertToFormat(QImage::Format_RGB888);
    }

    yodau::backend::frame f;
    f.width = img.width();
    f.height = img.height();
    f.stride = static_cast<int>(img.bytesPerLine());
    f.format = yodau::backend::pixel_format::rgb24;
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

    emit monitor_gui_frame_observed(
        stream_name, static_cast<qint64>(image.sizeInBytes())
    );

    if (!backend_runtime.processing_enabled()) {
        return;
    }

    const auto started = stream_controller_support::steady_clock::now();
    auto f = frame_from_image(scaled_processing_image(stream_name, image));
    stream_mgr->push_frame(stream_name.toStdString(), std::move(f));
    const auto finished = stream_controller_support::steady_clock::now();
    const auto elapsed_ms
        = std::chrono::duration<double, std::milli>(finished - started).count();
    note_processing_cost_sample(elapsed_ms);
    refresh_fps_policy(false);
}

void stream_controller::on_backend_event(const yodau::backend::event& e) {
    if (QThread::currentThread() != thread()) {
        const auto event_value = e;
        QMetaObject::invokeMethod(
            this, "on_backend_event_queued", Qt::QueuedConnection,
            Q_ARG(yodau::backend::event, event_value)
        );
        return;
    }

    on_backend_event_queued(e);
}

void stream_controller::on_backend_event_queued(
    yodau::backend::event event_value
) {
    const processing_feedback_state::processed_event feedback
        = feedback_state.consume_event(event_value);

    append_log(
        frontend_log_area::active, feedback.log_severity,
        QStringLiteral("backend_event"), feedback.log_message,
        feedback.stream_name, feedback.log_detail
    );

    emit monitor_backend_event_observed(
        feedback.kind_text
    );

    if (feedback.motion_activity_changed) {
        refresh_fps_policy(false);
    }

    auto* tile = widget_bridge.tile_for_stream_name(
        feedback.stream_name, route_state
    );
    if (!tile) {
        return;
    }

    if (!feedback.overlay_position_pct.has_value()
        || !feedback.allow_gui_overlay) {
        return;
    }

    if (feedback.tripwire_visual.has_value() && !feedback.line_name.isEmpty()) {
        tile->highlight_line_at(
            feedback.line_name, *feedback.overlay_position_pct,
            feedback.tripwire_visual->strength,
            feedback.tripwire_visual->direction,
            feedback.tripwire_visual->speed
        );
    }

    tile->add_event(*feedback.overlay_position_pct, feedback.overlay_color);
}
