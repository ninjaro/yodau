#include "shell/stream_controller.hpp"

#include "widgets/stream_cell.hpp"

#include <algorithm>
#include <chrono>

processing_sample stream_controller::active_processing_sample() const {
    const auto* cell = widget_bridge.active_cell();
    if (cell == nullptr || stream_mgr == nullptr
        || !stream_mgr->find_stream(cell->get_name().toStdString())) {
        return {};
    }

    processing_sample sample;
    sample.stream_name = cell->get_name();
    sample.source_status = cell->source_status();
    sample.application_active = cell->application_active();
    sample.processing_enabled = core_runtime.processing_enabled();
    const auto settings_value = settings_for_stream(sample.stream_name);
    sample.algorithm_id = settings_value.algorithm_id;
    sample.preset_id = settings_value.algorithm_preset;
    sample.metrics = runtime_metrics_by_stream.value(sample.stream_name);
    // The shared metrics also contain prepared-input dimensions. Only the
    // processed-frame callback establishes a delivered result's dimensions.
    sample.metrics.processed_width = 0;
    sample.metrics.processed_height = 0;
    const auto tracker = rate_trackers_by_stream.constFind(sample.stream_name);
    if (tracker != rate_trackers_by_stream.cend()) {
        const auto now = std::chrono::steady_clock::now();
        const auto age = [now](std::chrono::steady_clock::time_point time) {
            return time.time_since_epoch().count() == 0
                ? qint64(-1)
                : std::max<qint64>(
                      0,
                      std::chrono::duration_cast<std::chrono::milliseconds>(
                          now - time
                      )
                          .count()
                  );
        };
        sample.input_age_ms = age(tracker->last_input_frame);
        sample.result_age_ms = age(tracker->last_core_frame);
        if (sample.result_age_ms >= 0) {
            sample.metrics.processed_width = tracker->result_size.width();
            sample.metrics.processed_height = tracker->result_size.height();
        }
    }
    return sample;
}
