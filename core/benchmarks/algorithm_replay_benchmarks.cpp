#include "yodau_bench/scenario_catalog.hpp"

#include "analysis/default_processing_hooks.hpp"
#include "core/namespace_alias.hpp"
#include "geometry/geometry.hpp"
#include "streams/stream.hpp"

#include <benchmark/benchmark.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

namespace yodau::bench {

std::vector<scenario_spec> stage0_scenarios() {
    return {
        scenario_spec {
            .scenario_id = "single_day_sparse",
            .stream_count = 1,
            .lines_per_stream = 2,
            .frame_count = 120,
            .width = 640,
            .height = 360,
            .motion_objects = 1,
        },
        scenario_spec {
            .scenario_id = "single_day_dense",
            .stream_count = 1,
            .lines_per_stream = 6,
            .frame_count = 120,
            .width = 640,
            .height = 360,
            .motion_objects = 2,
        },
        scenario_spec {
            .scenario_id = "single_flicker_sparse",
            .stream_count = 1,
            .lines_per_stream = 2,
            .frame_count = 120,
            .width = 640,
            .height = 360,
            .motion_objects = 1,
            .lighting_flicker = true,
        },
        scenario_spec {
            .scenario_id = "single_shaky_sparse",
            .stream_count = 1,
            .lines_per_stream = 2,
            .frame_count = 120,
            .width = 640,
            .height = 360,
            .motion_objects = 1,
            .camera_shake_px = 6,
        },
        scenario_spec {
            .scenario_id = "multi4_day_mixed",
            .stream_count = 4,
            .lines_per_stream = 4,
            .frame_count = 90,
            .width = 480,
            .height = 270,
            .motion_objects = 2,
            .camera_shake_px = 2,
        },
        scenario_spec {
            .scenario_id = "multi9_day_mixed",
            .stream_count = 9,
            .lines_per_stream = 3,
            .frame_count = 60,
            .width = 320,
            .height = 180,
            .motion_objects = 1,
            .camera_shake_px = 1,
        },
        scenario_spec {
            .scenario_id = "crowded_dense_grid",
            .stream_count = 1,
            .lines_per_stream = 8,
            .frame_count = 120,
            .width = 640,
            .height = 360,
            .motion_objects = 6,
            .crowded_motion = true,
        },
    };
}

} // namespace yodau::bench

namespace {

using yodau::bench::scenario_spec;
using yodau::core::default_processing_algorithm_registry;
using yodau::core::frame;
using yodau::core::make_line;
using yodau::core::make_processing_algorithm;
using yodau::core::pixel_format;
using yodau::core::point;
using yodau::core::processing_algorithm;
using yodau::core::stream;

struct stream_replay {
    stream stream_value;
    std::vector<frame> frames;
    std::unordered_map<std::string, size_t> expected_tripwires_by_line;
    size_t expected_tripwire_count { 0 };
};

struct synthetic_object_state {
    int center_x { 0 };
    int center_y { 0 };
    int half_width { 0 };
    int half_height { 0 };
    point center_pct;
};

struct detected_tripwire_counts {
    std::unordered_map<std::string, size_t> by_line;
    size_t total { 0 };
    size_t unlabeled { 0 };
};

int clamped_channel(const int value) { return std::clamp(value, 0, 255); }

std::uint8_t byte_channel(const int value) {
    return static_cast<std::uint8_t>(clamped_channel(value));
}

int cycle_offset(const int phase, const int amplitude) {
    if (amplitude <= 0) {
        return 0;
    }

    static constexpr std::array<int, 8> pattern { 0, 1, 2, 1, 0, -1, -2, -1 };
    const size_t index
        = static_cast<size_t>(phase % static_cast<int>(pattern.size()));
    return pattern[index] * amplitude;
}

float pct_from_pixel(const int value, const int max_value) {
    if (max_value <= 1) {
        return 0.0f;
    }

    return static_cast<float>(
        100.0 * static_cast<double>(value) / static_cast<double>(max_value - 1)
    );
}

synthetic_object_state object_state(
    const scenario_spec& spec, const int stream_index, const int frame_index,
    const int object_index
) {
    const int shake_x
        = cycle_offset(frame_index + stream_index * 3, spec.camera_shake_px);
    const int shake_y = cycle_offset(
        frame_index * 2 + stream_index, std::max(1, spec.camera_shake_px / 2)
    );
    const int lane_count = spec.motion_objects + 1;
    const int center_y = ((object_index + 1) * spec.height) / lane_count
        + (spec.crowded_motion
               ? cycle_offset(frame_index + object_index * 5, 12)
               : cycle_offset(frame_index + object_index * 2, 4))
        + shake_y;
    const int travel = frame_index * (6 + object_index) + stream_index * 29
        + object_index * 43;
    const int center_x = travel % (spec.width + 96) - 48 + shake_x;
    const int half_width = spec.crowded_motion ? 14 : 18;
    const int half_height = spec.crowded_motion ? 10 : 14;

    return synthetic_object_state {
        .center_x = center_x,
        .center_y = center_y,
        .half_width = half_width,
        .half_height = half_height,
        .center_pct = point {
            .x = pct_from_pixel(center_x, spec.width),
            .y = pct_from_pixel(center_y, spec.height),
        },
    };
}

float cross_z(const point& a, const point& b, const point& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

int orientation(const point& a, const point& b, const point& c) {
    const float value = cross_z(a, b, c);
    if (std::fabs(value) <= point::epsilon) {
        return 0;
    }
    return value > 0.0f ? 1 : -1;
}

bool on_segment(const point& a, const point& b, const point& c) {
    return c.x <= std::max(a.x, b.x) + point::epsilon
        && c.x + point::epsilon >= std::min(a.x, b.x)
        && c.y <= std::max(a.y, b.y) + point::epsilon
        && c.y + point::epsilon >= std::min(a.y, b.y);
}

bool segments_intersect(
    const point& p1, const point& p2, const point& q1, const point& q2
) {
    const int o1 = orientation(p1, p2, q1);
    const int o2 = orientation(p1, p2, q2);
    const int o3 = orientation(q1, q2, p1);
    const int o4 = orientation(q1, q2, p2);

    if (o1 != o2 && o3 != o4) {
        return true;
    }
    if (o1 == 0 && on_segment(p1, p2, q1)) {
        return true;
    }
    if (o2 == 0 && on_segment(p1, p2, q2)) {
        return true;
    }
    if (o3 == 0 && on_segment(q1, q2, p1)) {
        return true;
    }
    if (o4 == 0 && on_segment(q1, q2, p2)) {
        return true;
    }

    return false;
}

double current_peak_rss_mb() {
#if defined(__unix__) || defined(__APPLE__)
    rusage usage {};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return 0.0;
    }
#if defined(__APPLE__)
    return static_cast<double>(usage.ru_maxrss) / (1024.0 * 1024.0);
#else
    return static_cast<double>(usage.ru_maxrss) / 1024.0;
#endif
#else
    return 0.0;
#endif
}

double p95_latency_ms(std::vector<double> latencies_ms) {
    if (latencies_ms.empty()) {
        return 0.0;
    }

    std::sort(latencies_ms.begin(), latencies_ms.end());
    const double scaled_index
        = 0.95 * static_cast<double>(latencies_ms.size() - 1);
    const size_t index = static_cast<size_t>(std::floor(scaled_index));
    return latencies_ms[index];
}

void write_pixel(
    frame& frame_value, const int x, const int y,
    const std::array<std::uint8_t, 3>& bgr
) {
    if (x < 0 || y < 0 || x >= frame_value.width || y >= frame_value.height) {
        return;
    }

    const size_t row_offset
        = static_cast<size_t>(y) * static_cast<size_t>(frame_value.stride);
    const size_t pixel_offset = static_cast<size_t>(x) * 3U;
    const size_t index = row_offset + pixel_offset;
    frame_value.data[index] = bgr[0];
    frame_value.data[index + 1U] = bgr[1];
    frame_value.data[index + 2U] = bgr[2];
}

void fill_background(
    frame& frame_value, const scenario_spec& spec, const int stream_index,
    const int frame_index
) {
    const int flicker = spec.lighting_flicker
        ? cycle_offset(frame_index + stream_index, 10)
        : 0;
    const int width_denominator = std::max(1, frame_value.width - 1);
    const int height_denominator = std::max(1, frame_value.height - 1);

    for (int y = 0; y < frame_value.height; ++y) {
        const int row_gradient = 18 + (y * 20) / height_denominator + flicker;
        for (int x = 0; x < frame_value.width; ++x) {
            const int column_gradient = (x * 16) / width_denominator;
            const int base = row_gradient + column_gradient;
            write_pixel(
                frame_value, x, y,
                {
                    byte_channel(base + 8),
                    byte_channel(base + 20),
                    byte_channel(base + 28),
                }
            );
        }
    }
}

void paint_rect(
    frame& frame_value, const int center_x, const int center_y,
    const int half_width, const int half_height,
    const std::array<std::uint8_t, 3>& bgr
) {
    const int min_x = center_x - half_width;
    const int max_x = center_x + half_width;
    const int min_y = center_y - half_height;
    const int max_y = center_y + half_height;

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            write_pixel(frame_value, x, y, bgr);
        }
    }
}

frame make_frame(
    const scenario_spec& spec, const int stream_index, const int frame_index
) {
    frame frame_value;
    frame_value.width = spec.width;
    frame_value.height = spec.height;
    frame_value.stride = spec.width * 3;
    frame_value.format = pixel_format::bgr24;
    frame_value.data.resize(
        static_cast<size_t>(frame_value.stride)
        * static_cast<size_t>(frame_value.height)
    );
    frame_value.ts
        = std::chrono::steady_clock::time_point {}
        + std::chrono::milliseconds(
              static_cast<long long>(frame_index * 33 + stream_index * 3)
        );

    fill_background(frame_value, spec, stream_index, frame_index);

    for (int object_index = 0; object_index < spec.motion_objects;
         ++object_index) {
        const synthetic_object_state state
            = object_state(spec, stream_index, frame_index, object_index);
        const int blue = 64 + (object_index * 29) % 128;
        const int green = 96 + (stream_index * 17 + object_index * 11) % 120;
        const int red = 180 + (object_index * 19) % 60;

        paint_rect(
            frame_value, state.center_x, state.center_y, state.half_width,
            state.half_height,
            {
                byte_channel(blue),
                byte_channel(green),
                byte_channel(red),
            }
        );
    }

    return frame_value;
}

stream make_stream(const scenario_spec& spec, const int stream_index) {
    stream stream_value(
        "benchmark://" + spec.scenario_id + "/" + std::to_string(stream_index),
        spec.scenario_id + "_stream_" + std::to_string(stream_index), "file",
        true
    );

    for (int line_index = 0; line_index < spec.lines_per_stream; ++line_index) {
        const bool vertical = line_index % 2 == 0;
        const float slot = 12.0f
            + static_cast<float>(line_index + 1) * 76.0f
                / static_cast<float>(spec.lines_per_stream + 1);
        std::vector<point> points;
        if (vertical) {
            points.push_back(point { .x = slot, .y = 8.0f });
            points.push_back(point { .x = slot, .y = 92.0f });
        } else {
            points.push_back(point { .x = 8.0f, .y = slot });
            points.push_back(point { .x = 92.0f, .y = slot });
        }

        stream_value.connect_line(make_line(
            std::move(points),
            stream_value.get_name() + "_line_" + std::to_string(line_index)
        ));
    }

    return stream_value;
}

void count_expected_tripwires(
    std::unordered_map<std::string, size_t>& counts_by_line,
    size_t& total_count, const stream& stream_value, const point& previous,
    const point& current
) {
    for (const auto& line_ptr_value : stream_value.lines_snapshot()) {
        if (!line_ptr_value || line_ptr_value->points.size() < 2) {
            continue;
        }

        bool intersected = false;
        for (size_t point_index = 1;
             point_index < line_ptr_value->points.size(); ++point_index) {
            if (segments_intersect(
                    previous, current, line_ptr_value->points[point_index - 1],
                    line_ptr_value->points[point_index]
                )) {
                intersected = true;
                break;
            }
        }

        if (!intersected && line_ptr_value->closed) {
            intersected = segments_intersect(
                previous, current, line_ptr_value->points.back(),
                line_ptr_value->points.front()
            );
        }

        if (!intersected) {
            continue;
        }

        ++counts_by_line[line_ptr_value->name];
        ++total_count;
    }
}

std::vector<stream_replay> build_replay(const scenario_spec& spec) {
    std::vector<stream_replay> replay;
    replay.reserve(static_cast<size_t>(spec.stream_count));

    for (int stream_index = 0; stream_index < spec.stream_count;
         ++stream_index) {
        stream_replay replay_value {
            .stream_value = make_stream(spec, stream_index),
            .frames = {},
            .expected_tripwires_by_line = {},
            .expected_tripwire_count = 0,
        };
        replay_value.frames.reserve(static_cast<size_t>(spec.frame_count));
        for (int frame_index = 0; frame_index < spec.frame_count;
             ++frame_index) {
            replay_value.frames.push_back(
                make_frame(spec, stream_index, frame_index)
            );
        }
        for (int frame_index = 1; frame_index < spec.frame_count;
             ++frame_index) {
            for (int object_index = 0; object_index < spec.motion_objects;
                 ++object_index) {
                const point previous
                    = object_state(
                          spec, stream_index, frame_index - 1, object_index
                    )
                          .center_pct;
                const point current
                    = object_state(
                          spec, stream_index, frame_index, object_index
                    )
                          .center_pct;
                count_expected_tripwires(
                    replay_value.expected_tripwires_by_line,
                    replay_value.expected_tripwire_count,
                    replay_value.stream_value, previous, current
                );
            }
        }
        replay.push_back(std::move(replay_value));
    }

    return replay;
}

void benchmark_algorithm_replay(
    benchmark::State& state, const std::string& algorithm_id,
    const scenario_spec& spec
) {
    const std::vector<stream_replay> replay = build_replay(spec);
    if (replay.empty() || replay.front().frames.empty()) {
        state.SkipWithError("Synthetic replay fixture was empty");
        return;
    }

    const size_t frames_per_iteration
        = replay.size() * replay.front().frames.size();

    double total_events = 0.0;
    double total_overlays = 0.0;
    double total_metrics = 0.0;
    double total_diagnostics = 0.0;
    double total_expected_tripwire_count = 0.0;
    double total_detected_tripwire_count = 0.0;
    double total_false_positive_count = 0.0;
    double total_missed_event_count = 0.0;
    double total_latency_ms = 0.0;
    double total_grid_cell_count = 0.0;
    double rss_mb = 0.0;
    size_t grid_cell_samples = 0;
    std::unordered_map<std::string, double> total_metric_value_by_name;
    std::unordered_map<std::string, size_t> metric_sample_count_by_name;
    std::vector<double> frame_latencies_ms;
    frame_latencies_ms.reserve(frames_per_iteration);

    for (auto _ : state) {
        std::vector<std::unique_ptr<processing_algorithm>> algorithms;
        algorithms.reserve(replay.size());
        std::vector<detected_tripwire_counts> detected_tripwires(replay.size());
        for (size_t stream_index = 0; stream_index < replay.size();
             ++stream_index) {
            auto algorithm = make_processing_algorithm(algorithm_id);
            if (!algorithm) {
                state.SkipWithError("Requested algorithm is unavailable");
                return;
            }
            algorithms.push_back(std::move(algorithm));
        }

        for (size_t frame_index = 0; frame_index < replay.front().frames.size();
             ++frame_index) {
            for (size_t stream_index = 0; stream_index < replay.size();
                 ++stream_index) {
                const auto wall_start = std::chrono::steady_clock::now();
                const auto result = algorithms[stream_index]->process_frame(
                    replay[stream_index].stream_value,
                    replay[stream_index].frames[frame_index]
                );
                const auto wall_end = std::chrono::steady_clock::now();

                const double latency_ms
                    = std::chrono::duration<double, std::milli>(
                          wall_end - wall_start
                    )
                          .count();
                frame_latencies_ms.push_back(latency_ms);
                total_latency_ms += latency_ms;

                total_events += static_cast<double>(result.events.size());
                total_overlays += static_cast<double>(result.overlays.size());
                total_metrics += static_cast<double>(result.metrics.size());
                total_diagnostics
                    += static_cast<double>(result.diagnostics.size());

                for (const auto& metric : result.metrics) {
                    if (metric.name == "grid_cell_count") {
                        total_grid_cell_count += metric.value;
                        ++grid_cell_samples;
                    }

                    total_metric_value_by_name[metric.name] += metric.value;
                    ++metric_sample_count_by_name[metric.name];
                }

                for (const auto& event_value : result.events) {
                    if (event_value.kind != yodau::core::event_kind::tripwire) {
                        continue;
                    }

                    ++detected_tripwires[stream_index].total;
                    if (event_value.line_name.empty()) {
                        ++detected_tripwires[stream_index].unlabeled;
                        continue;
                    }

                    ++detected_tripwires[stream_index]
                          .by_line[event_value.line_name];
                }
            }
        }

        for (size_t stream_index = 0; stream_index < replay.size();
             ++stream_index) {
            const auto& expected
                = replay[stream_index].expected_tripwires_by_line;
            const auto& detected = detected_tripwires[stream_index];

            total_expected_tripwire_count += static_cast<double>(
                replay[stream_index].expected_tripwire_count
            );
            total_detected_tripwire_count
                += static_cast<double>(detected.total);
            total_false_positive_count
                += static_cast<double>(detected.unlabeled);

            for (const auto& [line_name, detected_count] : detected.by_line) {
                const auto expected_it = expected.find(line_name);
                const size_t expected_count
                    = expected_it == expected.end() ? 0U : expected_it->second;
                if (detected_count > expected_count) {
                    total_false_positive_count
                        += static_cast<double>(detected_count - expected_count);
                }
            }

            for (const auto& [line_name, expected_count] : expected) {
                const auto detected_it = detected.by_line.find(line_name);
                const size_t detected_count
                    = detected_it == detected.by_line.end()
                    ? 0U
                    : detected_it->second;
                if (expected_count > detected_count) {
                    total_missed_event_count
                        += static_cast<double>(expected_count - detected_count);
                }
            }
        }

        rss_mb = std::max(rss_mb, current_peak_rss_mb());
    }

    const double iteration_count = static_cast<double>(state.iterations());
    const double processed_frames
        = static_cast<double>(frames_per_iteration) * iteration_count;
    const double total_line_count
        = static_cast<double>(spec.stream_count * spec.lines_per_stream);
    const double avg_latency_ms
        = processed_frames > 0.0 ? total_latency_ms / processed_frames : 0.0;

    state.counters["streams"] = static_cast<double>(spec.stream_count);
    state.counters["line_count"] = total_line_count;
    state.counters["frame_width"] = static_cast<double>(spec.width);
    state.counters["frame_height"] = static_cast<double>(spec.height);
    state.counters["processed_frames"] = processed_frames;
    state.counters["avg_fps"]
        = benchmark::Counter(processed_frames, benchmark::Counter::kIsRate);
    state.counters["frames_per_second"]
        = benchmark::Counter(processed_frames, benchmark::Counter::kIsRate);
    state.counters["avg_latency_ms"] = avg_latency_ms;
    state.counters["p95_latency_ms"] = p95_latency_ms(frame_latencies_ms);
    state.counters["rss_mb"] = rss_mb;
    state.counters["expected_tripwire_count"] = iteration_count > 0.0
        ? total_expected_tripwire_count / iteration_count
        : 0.0;
    state.counters["detected_tripwire_count"] = iteration_count > 0.0
        ? total_detected_tripwire_count / iteration_count
        : 0.0;
    state.counters["false_positive_count"] = iteration_count > 0.0
        ? total_false_positive_count / iteration_count
        : 0.0;
    state.counters["missed_event_count"] = iteration_count > 0.0
        ? total_missed_event_count / iteration_count
        : 0.0;
    if (grid_cell_samples > 0U) {
        state.counters["grid_cell_count"]
            = total_grid_cell_count / static_cast<double>(grid_cell_samples);
    }
    state.counters["avg_events_per_frame"]
        = processed_frames > 0.0 ? total_events / processed_frames : 0.0;
    state.counters["avg_overlays_per_frame"]
        = processed_frames > 0.0 ? total_overlays / processed_frames : 0.0;
    state.counters["avg_metrics_per_frame"]
        = processed_frames > 0.0 ? total_metrics / processed_frames : 0.0;
    state.counters["avg_diagnostics_per_frame"]
        = processed_frames > 0.0 ? total_diagnostics / processed_frames : 0.0;
    for (const auto& [metric_name, total_value] : total_metric_value_by_name) {
        const auto sample_it = metric_sample_count_by_name.find(metric_name);
        if (sample_it == metric_sample_count_by_name.end()
            || sample_it->second == 0U) {
            continue;
        }

        state.counters["metric_" + metric_name]
            = total_value / static_cast<double>(sample_it->second);
    }
}

void register_benchmarks() {
    const std::vector<std::string> algorithm_ids
        = default_processing_algorithm_registry().algorithm_ids();
    const std::vector<scenario_spec> scenarios
        = yodau::bench::stage0_scenarios();

    if (algorithm_ids.empty()) {
        benchmark::RegisterBenchmark(
            "replay/no_processing_algorithms", [](benchmark::State& state) {
                state.SkipWithError(
                    "No processing algorithms are registered; build with "
                    "OpenCV support"
                );
            }
        );
        return;
    }

    for (const std::string& algorithm_id : algorithm_ids) {
        for (const scenario_spec& spec : scenarios) {
            const std::string benchmark_name
                = "replay/" + algorithm_id + "/" + spec.scenario_id;
            benchmark::RegisterBenchmark(
                benchmark_name.c_str(),
                [algorithm_id, spec](benchmark::State& state) {
                    benchmark_algorithm_replay(state, algorithm_id, spec);
                }
            )->Unit(benchmark::kMillisecond);
        }
    }
}

[[maybe_unused]] const bool registered_benchmarks = [] {
    register_benchmarks();
    return true;
}();

} // namespace

BENCHMARK_MAIN();
