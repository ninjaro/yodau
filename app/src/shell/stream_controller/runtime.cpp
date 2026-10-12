#include "shell/stream_controller.hpp"
#include "streams/stream.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/stream_board.hpp"
#include "widgets/stream_cell.hpp"

#include <QImage>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace stream_controller_support {

using steady_clock = std::chrono::steady_clock;
constexpr auto fps_policy_refresh_interval = std::chrono::milliseconds(250);

} // namespace stream_controller_support

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
    tracker.result_size = QSize(width, height);
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
        return;
    }

    processing_cost_ema_ms
        = processing_cost_ema_ms * (1.0 - smoothing) + elapsed_ms * smoothing;
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
