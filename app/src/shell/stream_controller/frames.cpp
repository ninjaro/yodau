#include "shell/stream_controller.hpp"
#include "streams/event.hpp"
#include "streams/frame.hpp"
#include "widgets/stream_cell.hpp"

#include <QImage>
#include <QMetaObject>
#include <QThread>
#include <algorithm>
#include <chrono>
#include <utility>

namespace stream_controller_support {

using steady_clock = std::chrono::steady_clock;

} // namespace stream_controller_support

void stream_controller::on_core_events(
    const std::vector<yodau::core::event>& evs
) {
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
