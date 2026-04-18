#include "analysis/default_processing_algorithms.hpp"

#ifdef YODAU_OPENCV

#include "analysis/opencv_client.hpp"
#include "analysis/processing_algorithm_catalog.hpp"
#include "analysis/processing_algorithm_ids.hpp"

#include <utility>

namespace yodau::core {

namespace {

class motion_baseline_algorithm final : public processing_algorithm {
public:
    motion_baseline_algorithm() { configuration_ = default_configuration(); }

    std::string algorithm_id() const override {
        return processing_algorithm_ids::motion_baseline;
    }

    std::string display_name() const override {
        return processing_algorithm_display_name(algorithm_id());
    }

    processing_algorithm_configuration default_configuration() const override {
        return processing_algorithm_default_configuration(algorithm_id());
    }

    processing_algorithm_configuration configuration() const override {
        return configuration_;
    }

    void configure(processing_algorithm_configuration configuration) override {
        const auto defaults = default_configuration();
        for (const auto& [key, value] : defaults.values) {
            if (!configuration.values.contains(key)) {
                configuration.values.emplace(key, value);
            }
        }
        configuration_ = std::move(configuration);
    }

    void daemon_start(
        const stream& stream_value, const std::function<void(frame&&)>& on_frame,
        const std::stop_token& stop_token
    ) override {
        client_.daemon_start(stream_value, on_frame, stop_token);
    }

    processing_result process_frame(
        const stream& stream_value, const frame& frame_value
    ) override {
        processing_result result;
        result.events = client_.motion_processor(stream_value, frame_value);
        result.metrics.push_back(
            processing_metric {
                .name = "event_count",
                .value = static_cast<double>(result.events.size()),
                .unit = "events",
            }
        );
        result.metrics.push_back(
            processing_metric {
                .name = "line_count",
                .value = static_cast<double>(
                    stream_value.lines_snapshot().size()
                ),
                .unit = "lines",
            }
        );
        result.diagnostics.push_back(
            processing_diagnostic { .key = "algorithm", .value = algorithm_id() }
        );
        result.diagnostics.push_back(
            processing_diagnostic {
                .key = "display_name",
                .value = display_name(),
            }
        );
        return result;
    }

private:
    opencv_client client_;
    processing_algorithm_configuration configuration_;
};

} // namespace

std::unique_ptr<processing_algorithm> make_motion_baseline_algorithm() {
    return std::make_unique<motion_baseline_algorithm>();
}

} // namespace yodau::core

#endif
