#include "analysis/default_processing_algorithms.hpp"

#include "analysis/processing_algorithm_catalog.hpp"
#include "analysis/processing_algorithm_ids.hpp"
#include "analysis/processing_tripwire_tools.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef YODAU_OPENCV
#include "analysis/opencv_client.hpp"
#endif

namespace yodau::core {
namespace {

    constexpr int portable_max_sample_dimension = 64;
    constexpr int portable_difference_threshold = 25;
    constexpr double portable_minimum_motion_ratio = 0.02;
    constexpr auto portable_motion_cooldown = std::chrono::milliseconds(150);
    constexpr auto portable_tripwire_cooldown = std::chrono::milliseconds(1200);

    struct portable_sample {
        int width { 0 };
        int height { 0 };
        std::vector<std::uint8_t> gray;
    };

    std::uint8_t
    pixel_luma(const std::uint8_t* pixel, const pixel_format format) {
        if (format == pixel_format::gray8) {
            return pixel[0];
        }

        const int green = static_cast<int>(pixel[1]);
        int red = 0;
        int blue = 0;
        if (format == pixel_format::rgb24 || format == pixel_format::rgba32) {
            red = static_cast<int>(pixel[0]);
            blue = static_cast<int>(pixel[2]);
        } else {
            red = static_cast<int>(pixel[2]);
            blue = static_cast<int>(pixel[0]);
        }

        return static_cast<std::uint8_t>(
            (77 * red + 150 * green + 29 * blue + 128) >> 8
        );
    }

    std::optional<portable_sample>
    sample_frame_for_portable_processing(const frame& frame_value) {
        const frame_layout_validation layout
            = validate_frame_layout(frame_value);
        if (!layout) {
            return std::nullopt;
        }

        const int longest_side
            = std::max(frame_value.width, frame_value.height);
        const double scale = longest_side > portable_max_sample_dimension
            ? static_cast<double>(portable_max_sample_dimension)
                / static_cast<double>(longest_side)
            : 1.0;

        portable_sample sample;
        sample.width = std::max(
            1, static_cast<int>(std::lround(frame_value.width * scale))
        );
        sample.height = std::max(
            1, static_cast<int>(std::lround(frame_value.height * scale))
        );
        sample.gray.resize(
            static_cast<size_t>(sample.width)
            * static_cast<size_t>(sample.height)
        );

        const auto stride = static_cast<size_t>(frame_value.stride);
        for (int sample_y = 0; sample_y < sample.height; ++sample_y) {
            const int source_y = std::clamp(
                static_cast<int>(
                    ((2LL * sample_y + 1LL) * frame_value.height)
                    / (2LL * sample.height)
                ),
                0, frame_value.height - 1
            );
            for (int sample_x = 0; sample_x < sample.width; ++sample_x) {
                const int source_x = std::clamp(
                    static_cast<int>(
                        ((2LL * sample_x + 1LL) * frame_value.width)
                        / (2LL * sample.width)
                    ),
                    0, frame_value.width - 1
                );
                const size_t source_offset
                    = static_cast<size_t>(source_y) * stride
                    + static_cast<size_t>(source_x) * layout.bytes_per_pixel;
                sample.gray
                    [static_cast<size_t>(sample_y)
                         * static_cast<size_t>(sample.width)
                     + static_cast<size_t>(sample_x)]
                    = pixel_luma(
                        frame_value.data.data() + source_offset,
                        frame_value.format
                    );
            }
        }

        return sample;
    }

    std::chrono::steady_clock::time_point
    normalized_frame_timestamp(const frame& frame_value) {
        return frame_value.ts == std::chrono::steady_clock::time_point {}
            ? std::chrono::steady_clock::now()
            : frame_value.ts;
    }

    bool cooldown_elapsed(
        const std::chrono::steady_clock::time_point timestamp,
        const std::optional<std::chrono::steady_clock::time_point>& previous,
        const std::chrono::milliseconds cooldown
    ) {
        return !previous.has_value() || timestamp < *previous
            || timestamp - *previous >= cooldown;
    }

    event portable_motion_event(
        const std::string& stream_name,
        const std::chrono::steady_clock::time_point timestamp,
        const point position
    ) {
        event result;
        result.kind = event_kind::motion;
        result.stream_name = stream_name;
        result.ts = timestamp;
        result.pos_pct = position;
        return result;
    }

    double portable_impact_speed(
        const point previous, const point current, const double motion_ratio
    ) {
        const double dx = static_cast<double>(current.x - previous.x);
        const double dy = static_cast<double>(current.y - previous.y);
        const double distance_pct = std::hypot(dx, dy);
        const double ratio_boost = std::clamp(
            (motion_ratio - portable_minimum_motion_ratio) * 18.0, 0.0, 1.2
        );
        return std::clamp(0.35 + distance_pct / 8.0 + ratio_boost, 0.35, 2.5);
    }

    class portable_motion_baseline_algorithm final
        : public processing_algorithm {
    public:
        portable_motion_baseline_algorithm() {
            configuration_ = default_configuration();
        }

        std::string algorithm_id() const override {
            return processing_algorithm_ids::motion_baseline;
        }

        std::string display_name() const override {
            return processing_algorithm_display_name(algorithm_id());
        }

        processing_algorithm_configuration
        default_configuration() const override {
            return processing_algorithm_default_configuration(algorithm_id());
        }

        processing_algorithm_configuration configuration() const override {
            return configuration_;
        }

        void
        configure(processing_algorithm_configuration configuration) override {
            configuration_ = completed_processing_configuration(
                algorithm_id(), std::move(configuration)
            );
        }

        void daemon_start(
            const stream& stream_value,
            const std::function<void(frame&&)>& on_frame,
            const stop_token& stop_token
        ) override {
            (void)stream_value;
            (void)on_frame;
            (void)stop_token;
            throw std::runtime_error(
                "portable motion baseline requires frames supplied by an "
                "application client"
            );
        }

        processing_result process_frame(
            const stream& stream_value, const frame& frame_value
        ) override {
            processing_result result;
            const auto sample
                = sample_frame_for_portable_processing(frame_value);
            if (!sample.has_value()) {
                add_standard_result_fields(
                    result, stream_value, 0.0, 0, "invalid_frame"
                );
                return result;
            }

            std::scoped_lock lock(state_mutex_);
            if (!previous_sample_.has_value()
                || previous_sample_->width != sample->width
                || previous_sample_->height != sample->height) {
                previous_sample_ = sample;
                previous_motion_position_.reset();
                last_motion_emit_.reset();
                last_tripwire_emit_.clear();
                add_standard_result_fields(
                    result, stream_value, 0.0, sample->gray.size(), "warming"
                );
                return result;
            }

            size_t active_samples = 0;
            double position_sum_x = 0.0;
            double position_sum_y = 0.0;
            for (int y = 0; y < sample->height; ++y) {
                for (int x = 0; x < sample->width; ++x) {
                    const size_t index = static_cast<size_t>(y)
                            * static_cast<size_t>(sample->width)
                        + static_cast<size_t>(x);
                    const int difference = std::abs(
                        static_cast<int>(sample->gray[index])
                        - static_cast<int>(previous_sample_->gray[index])
                    );
                    if (difference < portable_difference_threshold) {
                        continue;
                    }

                    ++active_samples;
                    position_sum_x += (static_cast<double>(x) + 0.5) * 100.0
                        / static_cast<double>(sample->width);
                    position_sum_y += (static_cast<double>(y) + 0.5) * 100.0
                        / static_cast<double>(sample->height);
                }
            }
            previous_sample_ = sample;

            const size_t sample_count = sample->gray.size();
            const double motion_ratio = sample_count > 0
                ? static_cast<double>(active_samples)
                    / static_cast<double>(sample_count)
                : 0.0;
            const size_t minimum_active_samples = std::min(
                sample_count,
                std::max<size_t>(
                    2,
                    static_cast<size_t>(std::ceil(
                        static_cast<double>(sample_count)
                        * portable_minimum_motion_ratio
                    ))
                )
            );
            if (active_samples < minimum_active_samples) {
                add_standard_result_fields(
                    result, stream_value, motion_ratio, sample_count, "calm"
                );
                return result;
            }

            const auto timestamp = normalized_frame_timestamp(frame_value);
            if (!cooldown_elapsed(
                    timestamp, last_motion_emit_, portable_motion_cooldown
                )) {
                add_standard_result_fields(
                    result, stream_value, motion_ratio, sample_count,
                    "throttled"
                );
                return result;
            }
            last_motion_emit_ = timestamp;

            const point current_position {
                .x = static_cast<float>(
                    position_sum_x / static_cast<double>(active_samples)
                ),
                .y = static_cast<float>(
                    position_sum_y / static_cast<double>(active_samples)
                ),
            };

            if (previous_motion_position_.has_value()) {
                const double speed = portable_impact_speed(
                    *previous_motion_position_, current_position, motion_ratio
                );
                const auto crossings = tripwire_crossings_for_motion(
                    stream_value, *previous_motion_position_, current_position
                );
                for (const processing_tripwire_crossing& crossing : crossings) {
                    const std::string key = tripwire_crossing_key(crossing);
                    const auto previous_emit = last_tripwire_emit_.find(key);
                    const std::optional<std::chrono::steady_clock::time_point>
                        last_emit = previous_emit == last_tripwire_emit_.end()
                        ? std::optional<
                              std::chrono::steady_clock::time_point> {}
                        : std::optional(previous_emit->second);
                    if (!cooldown_elapsed(
                            timestamp, last_emit, portable_tripwire_cooldown
                        )) {
                        continue;
                    }
                    last_tripwire_emit_.insert_or_assign(key, timestamp);
                    result.events.push_back(make_tripwire_event(
                        stream_value.get_name(), crossing, timestamp, speed
                    ));
                }
            }
            previous_motion_position_ = current_position;
            result.events.push_back(portable_motion_event(
                stream_value.get_name(), timestamp, current_position
            ));

            add_standard_result_fields(
                result, stream_value, motion_ratio, sample_count, "motion"
            );
            return result;
        }

    private:
        void add_standard_result_fields(
            processing_result& result, const stream& stream_value,
            const double motion_ratio, const size_t sample_count,
            const std::string& frame_state
        ) const {
            add_processing_metric(
                result, "event_count",
                static_cast<double>(result.events.size()), "events"
            );
            add_processing_metric(
                result, "line_count",
                static_cast<double>(stream_value.lines_snapshot().size()),
                "lines"
            );
            add_processing_metric(
                result, "motion_ratio", motion_ratio, "ratio"
            );
            add_processing_metric(
                result, "portable_sample_count",
                static_cast<double>(sample_count), "samples"
            );
            add_algorithm_diagnostics(result, *this);
            add_processing_diagnostic(
                result, "processing_backend", "portable_frame_delta"
            );
            add_processing_diagnostic(result, "frame_state", frame_state);
        }

        processing_algorithm_configuration configuration_;
        std::optional<portable_sample> previous_sample_;
        std::optional<point> previous_motion_position_;
        std::optional<std::chrono::steady_clock::time_point> last_motion_emit_;
        std::unordered_map<std::string, std::chrono::steady_clock::time_point>
            last_tripwire_emit_;
        std::mutex state_mutex_;
    };

#ifdef YODAU_OPENCV
    class opencv_motion_baseline_algorithm final : public processing_algorithm {
    public:
        opencv_motion_baseline_algorithm() {
            configuration_ = default_configuration();
        }

        std::string algorithm_id() const override {
            return processing_algorithm_ids::motion_baseline;
        }

        std::string display_name() const override {
            return processing_algorithm_display_name(algorithm_id());
        }

        processing_algorithm_configuration
        default_configuration() const override {
            return processing_algorithm_default_configuration(algorithm_id());
        }

        processing_algorithm_configuration configuration() const override {
            return configuration_;
        }

        void
        configure(processing_algorithm_configuration configuration) override {
            configuration_ = completed_processing_configuration(
                algorithm_id(), std::move(configuration)
            );
        }

        void daemon_start(
            const stream& stream_value,
            const std::function<void(frame&&)>& on_frame,
            const stop_token& stop_token
        ) override {
            opencv_client::daemon_start(stream_value, on_frame, stop_token);
        }

        processing_result process_frame(
            const stream& stream_value, const frame& frame_value
        ) override {
            processing_result result;
            result.events = client_.motion_processor(stream_value, frame_value);
            add_processing_metric(
                result, "event_count",
                static_cast<double>(result.events.size()), "events"
            );
            add_processing_metric(
                result, "line_count",
                static_cast<double>(stream_value.lines_snapshot().size()),
                "lines"
            );
            add_algorithm_diagnostics(result, *this);
            add_processing_diagnostic(result, "processing_backend", "opencv");
            return result;
        }

    private:
        opencv_client client_;
        processing_algorithm_configuration configuration_;
    };
#endif

} // namespace

std::unique_ptr<processing_algorithm>
make_portable_motion_baseline_algorithm() {
    return std::make_unique<portable_motion_baseline_algorithm>();
}

std::unique_ptr<processing_algorithm> make_motion_baseline_algorithm() {
#ifdef YODAU_OPENCV
    return std::make_unique<opencv_motion_baseline_algorithm>();
#else
    return make_portable_motion_baseline_algorithm();
#endif
}

} // namespace yodau::core
