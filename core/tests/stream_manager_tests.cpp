#include "analysis/default_processing_hooks.hpp"
#include "analysis/processing_algorithm.hpp"
#include "analysis/processing_runtime.hpp"
#include "analysis/tripwire_grid_stream_index.hpp"
#include "geometry/coords.hpp"
#include "geometry/geometry.hpp"
#include "streams/analysis_scheduler.hpp"
#include "streams/stream_manager.hpp"
#include "streams/virtual_camera.hpp"

#include <gtest/gtest.h>

#include <functional>
#include <set>
#include <thread>
#include <vector>

using yodau::backend::build_grid_stream_index;
using yodau::backend::collect_grid_candidates;
using yodau::backend::default_processing_algorithm_id;
using yodau::backend::default_processing_algorithm_registry;
using yodau::backend::grid_candidate_tracker;
using yodau::backend::grid_dims;
using yodau::backend::grid_index;
using yodau::backend::analysis_scheduler;
using yodau::backend::line_profile;
using yodau::backend::line_ptr;
using yodau::backend::make_line;
using yodau::backend::make_line_profile;
using yodau::backend::point;
using yodau::backend::processing_algorithm;
using yodau::backend::processing_algorithm_configuration;
using yodau::backend::processing_algorithm_registry;
using yodau::backend::processing_metric;
using yodau::backend::processing_result;
using yodau::backend::processing_runtime;
using yodau::backend::processing_runtime_options;
using yodau::backend::recommend_grid_dims;
using yodau::backend::render_mode;
using yodau::backend::stream;
using yodau::backend::stream_manager;
using yodau::backend::trace_grid_cells_pct;
using yodau::backend::virtual_camera;
using yodau::backend::virtual_camera_frame_info;

namespace stream_manager_tests_support {

struct processed_frame_probe {
    size_t call_count { 0 };
    std::string last_stream_name;
    int last_frame_width { 0 };
    size_t last_event_count { 0 };

    std::vector<yodau::backend::event> process(
        const stream& stream_value, const yodau::backend::frame& frame_value
    ) {
        yodau::backend::event event_value;
        event_value.kind = yodau::backend::event_kind::motion;
        event_value.stream_name = stream_value.get_name();
        event_value.ts = frame_value.ts;
        event_value.pos_pct = point { 25.0f, 50.0f };
        return { event_value };
    }

    void on_processed_frame(
        const stream& stream_value, const yodau::backend::frame& frame_value,
        const std::vector<yodau::backend::event>& events
    ) {
        call_count += 1;
        last_stream_name = stream_value.get_name();
        last_frame_width = frame_value.width;
        last_event_count = events.size();
    }
};

class dummy_processing_algorithm final : public processing_algorithm {
public:
    std::string algorithm_id() const override { return "dummy"; }

    std::string display_name() const override { return "dummy algorithm"; }

    processing_algorithm_configuration default_configuration() const override {
        processing_algorithm_configuration configuration;
        configuration.values.emplace("mode", "dummy");
        return configuration;
    }

    processing_algorithm_configuration configuration() const override {
        return configuration_;
    }

    void configure(processing_algorithm_configuration configuration) override {
        configuration_ = std::move(configuration);
    }

    void daemon_start(
        const stream& stream_value,
        const std::function<void(yodau::backend::frame&&)>& on_frame,
        const std::stop_token& stop_token
    ) override {
        (void)stream_value;
        (void)on_frame;
        (void)stop_token;
    }

    processing_result process_frame(
        const stream& stream_value, const yodau::backend::frame& frame_value
    ) override {
        (void)frame_value;

        processing_result result;
        result.metrics.push_back(
            processing_metric {
                .name = "stream_name_size",
                .value = static_cast<double>(stream_value.get_name().size()),
                .unit = "chars",
            }
        );
        return result;
    }

private:
    processing_algorithm_configuration configuration_ = default_configuration();
};

std::string diagnostic_value(
    const processing_result& result, const std::string& key
) {
    for (const auto& diagnostic : result.diagnostics) {
        if (diagnostic.key == key) {
            return diagnostic.value;
        }
    }

    return {};
}

yodau::backend::frame make_gray_frame(
    const int width, const int height,
    const std::chrono::steady_clock::time_point timestamp,
    const int bright_left = -1, const int bright_top = -1,
    const int bright_right = -1, const int bright_bottom = -1
) {
    yodau::backend::frame frame_value;
    frame_value.width = width;
    frame_value.height = height;
    frame_value.stride = width;
    frame_value.format = yodau::backend::pixel_format::gray8;
    frame_value.ts = timestamp;
    frame_value.data.assign(
        static_cast<size_t>(width * height), static_cast<std::uint8_t>(0)
    );

    if (bright_left < 0 || bright_top < 0 || bright_right <= bright_left
        || bright_bottom <= bright_top) {
        return frame_value;
    }

    for (int y = bright_top; y < bright_bottom; ++y) {
        for (int x = bright_left; x < bright_right; ++x) {
            frame_value.data[static_cast<size_t>(y * width + x)] = 255;
        }
    }

    return frame_value;
}

yodau::backend::frame make_rgb_frame(
    const int width, const int height,
    const std::chrono::steady_clock::time_point timestamp,
    const int bright_left = -1, const int bright_top = -1,
    const int bright_right = -1, const int bright_bottom = -1
) {
    yodau::backend::frame frame_value;
    frame_value.width = width;
    frame_value.height = height;
    frame_value.stride = width * 3;
    frame_value.format = yodau::backend::pixel_format::rgb24;
    frame_value.ts = timestamp;
    frame_value.data.assign(
        static_cast<size_t>(frame_value.stride * height),
        static_cast<std::uint8_t>(0)
    );

    if (bright_left < 0 || bright_top < 0 || bright_right <= bright_left
        || bright_bottom <= bright_top) {
        return frame_value;
    }

    for (int y = bright_top; y < bright_bottom; ++y) {
        for (int x = bright_left; x < bright_right; ++x) {
            const size_t pixel_offset
                = static_cast<size_t>(y * frame_value.stride + x * 3);
            frame_value.data[pixel_offset + 0] = 255;
            frame_value.data[pixel_offset + 1] = 255;
            frame_value.data[pixel_offset + 2] = 255;
        }
    }

    return frame_value;
}

} // namespace stream_manager_tests_support

TEST(line_profile_test, make_line_profile_normalizes_future_semantic_fields) {
    const line_profile profile = make_line_profile("north", 4.5f, 0.0f, 0.0f, 2.0f);

    EXPECT_EQ(profile.line_name, "north");
    EXPECT_FLOAT_EQ(profile.visual_width, 4.5f);
    EXPECT_FLOAT_EQ(profile.interaction_width, 4.5f);
    EXPECT_FLOAT_EQ(profile.effective_length, 1.0f);
    EXPECT_FLOAT_EQ(profile.damping, 1.0f);
}

TEST(analysis_scheduler_test, overrides_and_clear_restore_default_interval) {
    analysis_scheduler scheduler;
    scheduler.set_default_interval_ms(200);

    const analysis_scheduler::time_point t0 {};
    EXPECT_EQ(scheduler.interval_for_stream("cam0"), 200);
    EXPECT_TRUE(scheduler.should_process("cam0", t0));
    EXPECT_FALSE(
        scheduler.should_process("cam0", t0 + std::chrono::milliseconds(100))
    );

    scheduler.set_stream_interval_ms("cam0", 50);
    EXPECT_EQ(scheduler.interval_for_stream("cam0"), 50);
    EXPECT_TRUE(
        scheduler.should_process("cam0", t0 + std::chrono::milliseconds(100))
    );
    EXPECT_FALSE(
        scheduler.should_process("cam0", t0 + std::chrono::milliseconds(140))
    );

    scheduler.clear_stream_interval_ms("cam0");
    EXPECT_EQ(scheduler.interval_for_stream("cam0"), 200);
    EXPECT_FALSE(
        scheduler.should_process("cam0", t0 + std::chrono::milliseconds(250))
    );
    EXPECT_TRUE(
        scheduler.should_process("cam0", t0 + std::chrono::milliseconds(300))
    );
}

TEST(processing_algorithm_registry_test, registry_creates_named_algorithm) {
    processing_algorithm_registry registry;

    EXPECT_TRUE(
        registry.register_algorithm(
            processing_algorithm_registry::entry {
                .algorithm_id = "dummy",
                .display_name = "dummy algorithm",
                .create = [] {
                    return std::make_unique<
                        stream_manager_tests_support::dummy_processing_algorithm>();
                },
            }
        )
    );
    EXPECT_FALSE(
        registry.register_algorithm(
            processing_algorithm_registry::entry {
                .algorithm_id = "dummy",
                .display_name = "dummy duplicate",
                .create = [] {
                    return std::make_unique<
                        stream_manager_tests_support::dummy_processing_algorithm>();
                },
            }
        )
    );

    const std::vector<std::string> ids = registry.algorithm_ids();
    ASSERT_EQ(ids.size(), 1u);
    EXPECT_EQ(ids.front(), "dummy");

    auto algorithm = registry.create("dummy");
    ASSERT_TRUE(algorithm);
    EXPECT_EQ(algorithm->algorithm_id(), "dummy");
    EXPECT_EQ(algorithm->display_name(), "dummy algorithm");

    processing_algorithm_configuration configuration;
    configuration.values.emplace("threshold", "7");
    algorithm->configure(configuration);

    EXPECT_EQ(algorithm->configuration().values.at("threshold"), "7");

    const processing_result result = algorithm->process_frame(
        stream("/tmp/cam.mp4", "cam0", "file", true),
        yodau::backend::frame {}
    );
    ASSERT_EQ(result.metrics.size(), 1u);
    EXPECT_EQ(result.metrics.front().name, "stream_name_size");
    EXPECT_EQ(result.metrics.front().unit, "chars");
    EXPECT_DOUBLE_EQ(result.metrics.front().value, 4.0);
}

#ifdef YODAU_OPENCV
TEST(processing_algorithm_registry_test, default_registry_exposes_motion_baseline) {
    const auto& registry = default_processing_algorithm_registry();

    EXPECT_TRUE(registry.contains(default_processing_algorithm_id()));
    EXPECT_TRUE(registry.contains("spot_grid"));
    EXPECT_TRUE(registry.contains("spot grid"));
    EXPECT_TRUE(registry.contains("contour_mask"));
    EXPECT_TRUE(registry.contains("contour mask"));

    auto algorithm = registry.create(default_processing_algorithm_id());
    ASSERT_TRUE(algorithm);
    EXPECT_EQ(algorithm->algorithm_id(), "motion_baseline");
    EXPECT_EQ(algorithm->display_name(), "motion baseline");

    auto spot_grid_algorithm = registry.create("spot grid");
    ASSERT_TRUE(spot_grid_algorithm);
    EXPECT_EQ(spot_grid_algorithm->algorithm_id(), "spot_grid");
    EXPECT_EQ(spot_grid_algorithm->display_name(), "spot grid");

    auto contour_mask_algorithm = registry.create("contour mask");
    ASSERT_TRUE(contour_mask_algorithm);
    EXPECT_EQ(contour_mask_algorithm->algorithm_id(), "contour_mask");
    EXPECT_EQ(contour_mask_algorithm->display_name(), "contour mask");
}

TEST(processing_runtime_test, keeps_default_and_per_stream_algorithm_choices) {
    processing_runtime runtime(
        processing_runtime_options {
            .mode = render_mode::frontend_only,
            .enable_virtual_camera = false,
            .algorithm_id = {},
        }
    );

    EXPECT_EQ(runtime.default_algorithm_id(), "motion_baseline");
    EXPECT_EQ(runtime.algorithm_id_for_stream("cam-a"), "motion_baseline");
    EXPECT_TRUE(runtime.set_default_algorithm("spot"));
    EXPECT_EQ(runtime.default_algorithm_id(), "spot_grid");
    EXPECT_EQ(runtime.algorithm_id_for_stream("cam-a"), "spot_grid");

    EXPECT_TRUE(runtime.set_stream_algorithm("cam-b", "contour"));
    EXPECT_EQ(runtime.algorithm_id_for_stream("cam-b"), "contour_mask");

    const auto overrides = runtime.stream_algorithm_overrides();
    ASSERT_EQ(overrides.size(), 1u);
    EXPECT_EQ(overrides.at("cam-b"), "contour_mask");

    EXPECT_TRUE(runtime.clear_stream_algorithm("cam-b"));
    EXPECT_EQ(runtime.algorithm_id_for_stream("cam-b"), "spot_grid");
}

TEST(processing_runtime_test, processes_each_stream_with_its_selected_algorithm) {
    processing_runtime runtime(
        processing_runtime_options {
            .mode = render_mode::frontend_only,
            .enable_virtual_camera = false,
            .algorithm_id = "spot_grid",
        }
    );

    ASSERT_TRUE(runtime.set_stream_algorithm("cam-contour", "contour_mask"));

    const auto frame_processor = runtime.frame_processor_hook();
    ASSERT_TRUE(static_cast<bool>(frame_processor));

    const auto timestamp = std::chrono::steady_clock::now();
    const auto dark_frame = stream_manager_tests_support::make_gray_frame(
        64, 64, timestamp
    );
    const auto bright_frame = stream_manager_tests_support::make_gray_frame(
        64, 64, timestamp + std::chrono::milliseconds(200), 18, 18, 42, 42
    );

    const stream spot_stream("/tmp/spot.mp4", "cam-spot", "file", true);
    const stream contour_stream(
        "/tmp/contour.mp4", "cam-contour", "file", true
    );

    EXPECT_TRUE(frame_processor(spot_stream, dark_frame).empty());
    EXPECT_FALSE(frame_processor(spot_stream, bright_frame).empty());
    EXPECT_TRUE(frame_processor(contour_stream, dark_frame).empty());
    EXPECT_FALSE(frame_processor(contour_stream, bright_frame).empty());

    const auto spot_result = runtime.latest_processing_result("cam-spot");
    ASSERT_TRUE(spot_result.has_value());
    EXPECT_EQ(
        stream_manager_tests_support::diagnostic_value(
            *spot_result, "algorithm"
        ),
        "spot_grid"
    );

    const auto contour_result = runtime.latest_processing_result("cam-contour");
    ASSERT_TRUE(contour_result.has_value());
    EXPECT_EQ(
        stream_manager_tests_support::diagnostic_value(
            *contour_result, "algorithm"
        ),
        "contour_mask"
    );
}

TEST(processing_runtime_test, backend_preview_router_publishes_rendered_frame) {
    processing_runtime runtime(
        processing_runtime_options {
            .mode = render_mode::backend_only,
            .enable_virtual_camera = true,
            .algorithm_id = "spot_grid",
        }
    );

    ASSERT_TRUE(runtime.has_virtual_camera());

    const auto frame_processor = runtime.frame_processor_hook();
    const auto frame_sink = runtime.processed_frame_sink();
    ASSERT_TRUE(static_cast<bool>(frame_processor));
    ASSERT_TRUE(static_cast<bool>(frame_sink));

    stream preview_stream("/tmp/preview.mp4", "cam-preview", "file", true);
    preview_stream.connect_line(
        make_line(
            { point { 10.0f, 50.0f }, point { 90.0f, 50.0f } }, "guide"
        )
    );

    const auto timestamp = std::chrono::steady_clock::now();
    const auto dark_frame = stream_manager_tests_support::make_gray_frame(
        64, 64, timestamp
    );
    const auto bright_frame = stream_manager_tests_support::make_gray_frame(
        64, 64, timestamp + std::chrono::milliseconds(200), 18, 18, 42, 42
    );

    EXPECT_TRUE(frame_processor(preview_stream, dark_frame).empty());
    const auto events = frame_processor(preview_stream, bright_frame);
    ASSERT_FALSE(events.empty());

    frame_sink(preview_stream, bright_frame, events);

    const virtual_camera* preview_camera = runtime.preview_camera();
    ASSERT_NE(preview_camera, nullptr);
    const auto published_frame
        = preview_camera->latest_frame(preview_stream.get_name());
    ASSERT_TRUE(published_frame.has_value());
    EXPECT_EQ(published_frame->width, bright_frame.width);
    EXPECT_EQ(published_frame->height, bright_frame.height);
    EXPECT_EQ(published_frame->format, yodau::backend::pixel_format::bgr24);
    EXPECT_FALSE(published_frame->data.empty());
    EXPECT_NE(published_frame->data, bright_frame.data);
}

TEST(
    processing_runtime_test,
    motion_baseline_warm_resets_when_stream_frame_size_changes
) {
    processing_runtime runtime(
        processing_runtime_options {
            .mode = render_mode::frontend_only,
            .enable_virtual_camera = false,
            .algorithm_id = "motion_baseline",
        }
    );

    const auto frame_processor = runtime.frame_processor_hook();
    ASSERT_TRUE(static_cast<bool>(frame_processor));

    const auto timestamp = std::chrono::steady_clock::now();
    const auto first_frame = stream_manager_tests_support::make_rgb_frame(
        64, 64, timestamp
    );
    const auto resized_frame = stream_manager_tests_support::make_rgb_frame(
        48, 32, timestamp + std::chrono::milliseconds(200)
    );
    const auto resumed_frame = stream_manager_tests_support::make_rgb_frame(
        48, 32, timestamp + std::chrono::milliseconds(400), 8, 6, 40, 26
    );

    const stream motion_stream("/tmp/motion.mp4", "cam-motion", "file", true);

    EXPECT_NO_THROW((void)frame_processor(motion_stream, first_frame));
    EXPECT_NO_THROW((void)frame_processor(motion_stream, resized_frame));
    EXPECT_NO_THROW((void)frame_processor(motion_stream, resumed_frame));

    const auto latest = runtime.latest_processing_result("cam-motion");
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(
        stream_manager_tests_support::diagnostic_value(
            *latest, "algorithm"
        ),
        "motion_baseline"
    );
}
#endif

TEST(
    tripwire_grid_stream_index_test,
    recommend_grid_dims_focuses_on_new_small_line
) {
    std::vector<line_ptr> lines {
        make_line({ point { 5.0f, 5.0f }, point { 95.0f, 5.0f } }, "top"),
        make_line({ point { 5.0f, 95.0f }, point { 95.0f, 95.0f } }, "bottom"),
        make_line({ point { 5.0f, 5.0f }, point { 5.0f, 95.0f } }, "left"),
        make_line({ point { 95.0f, 5.0f }, point { 95.0f, 95.0f } }, "right"),
        make_line({ point { 10.0f, 20.0f }, point { 90.0f, 20.0f } }, "mid_a"),
        make_line({ point { 10.0f, 80.0f }, point { 90.0f, 80.0f } }, "mid_b"),
    };

    const line_ptr focus_line = make_line(
        { point { 48.0f, 49.0f }, point { 51.0f, 51.0f } }, "focus"
    );
    lines.push_back(focus_line);

    const grid_dims neutral = recommend_grid_dims(lines);
    const grid_dims focused = recommend_grid_dims(lines, focus_line.get());

    EXPECT_GE(focused.nx, neutral.nx);
    EXPECT_GE(focused.ny, neutral.ny);
    EXPECT_GT(focused.nx * focused.ny, neutral.nx * neutral.ny);
}

TEST(
    tripwire_grid_stream_index_test,
    collect_grid_candidates_stays_local_to_active_cells
) {
    const std::vector<line_ptr> lines {
        make_line({ point { 10.0f, 10.0f }, point { 10.0f, 90.0f } }, "left"),
        make_line({ point { 50.0f, 10.0f }, point { 50.0f, 90.0f } }, "center"),
        make_line({ point { 90.0f, 10.0f }, point { 90.0f, 90.0f } }, "right"),
    };

    const grid_dims dims { 12, 12 };
    const auto index = build_grid_stream_index(lines, dims);

    std::vector<int> active_cell_indices;
    for (const auto& cell : trace_grid_cells_pct(
             point { 7.0f, 15.0f }, point { 14.0f, 85.0f }, dims
         )) {
        active_cell_indices.push_back(grid_index(cell, dims));
    }

    grid_candidate_tracker tracker;
    std::vector<size_t> candidate_segment_ids;
    collect_grid_candidates(
        index, active_cell_indices, tracker, candidate_segment_ids
    );

    ASSERT_FALSE(candidate_segment_ids.empty());
    EXPECT_LT(candidate_segment_ids.size(), index.segments.size());

    std::set<std::string> candidate_line_names;
    for (const size_t segment_id : candidate_segment_ids) {
        ASSERT_LT(segment_id, index.segments.size());
        const auto& ref = index.segments[segment_id];
        ASSERT_LT(ref.line_index, index.lines.size());
        candidate_line_names.insert(index.lines[ref.line_index].name);
    }

    EXPECT_EQ(candidate_line_names, std::set<std::string> { "left" });
}

TEST(
    tripwire_grid_stream_index_test,
    collect_grid_candidates_deduplicates_segment_ids
) {
    const std::vector<line_ptr> lines {
        make_line({ point { 0.0f, 0.0f }, point { 100.0f, 100.0f } }, "diag"),
    };

    const grid_dims dims { 10, 10 };
    const auto index = build_grid_stream_index(lines, dims);

    std::vector<int> active_cell_indices;
    for (const auto& cell : trace_grid_cells_pct(
             point { 0.0f, 0.0f }, point { 100.0f, 100.0f }, dims
         )) {
        active_cell_indices.push_back(grid_index(cell, dims));
    }

    grid_candidate_tracker tracker;
    std::vector<size_t> candidate_segment_ids;
    collect_grid_candidates(
        index, active_cell_indices, tracker, candidate_segment_ids
    );

    ASSERT_EQ(index.segments.size(), 1u);
    ASSERT_EQ(candidate_segment_ids.size(), 1u);
    EXPECT_EQ(candidate_segment_ids.front(), 0u);
}

TEST(stream_manager_test, processed_frame_sink_receives_frame_and_events) {
    stream_manager mgr;
    mgr.add_stream("/tmp/cam.mp4", "cam0", "file", true);

    stream_manager_tests_support::processed_frame_probe probe;
    mgr.set_frame_processor(
        std::bind_front(
            &stream_manager_tests_support::processed_frame_probe::process,
            &probe
        )
    );
    mgr.set_processed_frame_sink(
        std::bind_front(
            &stream_manager_tests_support::processed_frame_probe::
                on_processed_frame,
            &probe
        )
    );

    yodau::backend::frame frame_value;
    frame_value.width = 320;
    frame_value.height = 180;
    frame_value.stride = 320 * 3;
    frame_value.format = yodau::backend::pixel_format::rgb24;
    frame_value.data.resize(static_cast<size_t>(frame_value.stride) * 180u);
    frame_value.ts = std::chrono::steady_clock::now();

    mgr.push_frame("cam0", std::move(frame_value));

    EXPECT_EQ(probe.call_count, 1u);
    EXPECT_EQ(probe.last_stream_name, "cam0");
    EXPECT_EQ(probe.last_frame_width, 320);
    EXPECT_EQ(probe.last_event_count, 1u);
}

TEST(
    stream_manager_test,
    analysis_interval_override_throttles_and_clear_restores_default
) {
    stream_manager mgr;
    mgr.add_stream("/tmp/cam.mp4", "cam0", "file", true);
    mgr.set_analysis_interval_ms(1000);

    stream_manager_tests_support::processed_frame_probe probe;
    mgr.set_frame_processor(
        std::bind_front(
            &stream_manager_tests_support::processed_frame_probe::process,
            &probe
        )
    );

    yodau::backend::frame frame_value;
    frame_value.ts = std::chrono::steady_clock::now();

    EXPECT_EQ(mgr.process_frame("cam0", yodau::backend::frame(frame_value)).size(), 1u);
    EXPECT_TRUE(mgr.process_frame("cam0", yodau::backend::frame(frame_value)).empty());

    mgr.set_stream_analysis_interval_ms("cam0", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_EQ(mgr.process_frame("cam0", yodau::backend::frame(frame_value)).size(), 1u);

    mgr.clear_stream_analysis_interval_ms("cam0");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_TRUE(mgr.process_frame("cam0", yodau::backend::frame(frame_value)).empty());
}

TEST(stream_manager_test, line_profile_is_stored_beside_geometry_line) {
    stream_manager mgr;

    const line_ptr line_value = mgr.add_line("0,0;100,0", false, "north");
    ASSERT_TRUE(line_value);
    EXPECT_EQ(line_value->name, "north");
    EXPECT_EQ(line_value->points.size(), 2u);
    EXPECT_FALSE(line_value->closed);

    const auto default_profile = mgr.find_line_profile("north");
    ASSERT_TRUE(default_profile.has_value());
    EXPECT_EQ(default_profile->line_name, "north");
    EXPECT_FLOAT_EQ(default_profile->visual_width, 1.0f);
    EXPECT_FLOAT_EQ(default_profile->interaction_width, 1.0f);
    EXPECT_FLOAT_EQ(default_profile->effective_length, 1.0f);
    EXPECT_FLOAT_EQ(default_profile->damping, 0.5f);

    mgr.set_line_profile(line_profile {
        .line_name = "north",
        .visual_width = 6.0f,
        .interaction_width = 0.0f,
        .effective_length = 2.5f,
        .damping = -1.0f,
    });

    const auto stored_profile = mgr.find_line_profile("north");
    ASSERT_TRUE(stored_profile.has_value());
    EXPECT_EQ(
        *stored_profile,
        make_line_profile("north", 6.0f, 0.0f, 2.5f, -1.0f)
    );

    EXPECT_EQ(line_value->name, "north");
    EXPECT_EQ(line_value->points.size(), 2u);
    EXPECT_FALSE(line_value->closed);
}

TEST(
    stream_manager_test,
    stream_specific_line_profile_override_does_not_mutate_global_line_profile
) {
    stream_manager mgr;

    mgr.add_stream("/tmp/cam.mp4", "cam0", "file", true);
    mgr.add_line("0,0;100,0", false, "north");
    mgr.set_line_profile(make_line_profile("north", 6.0f, 7.0f, 1.4f, 0.75f));
    mgr.set_line("cam0", "north");

    const auto inherited_profile
        = mgr.find_stream_line_profile("cam0", "north");
    ASSERT_TRUE(inherited_profile.has_value());
    EXPECT_EQ(
        *inherited_profile,
        make_line_profile("north", 6.0f, 7.0f, 1.4f, 0.75f)
    );

    mgr.set_stream_line_profile(
        "cam0", make_line_profile("north", 2.0f, 2.0f, 0.75f, 0.25f)
    );

    const auto global_profile = mgr.find_line_profile("north");
    ASSERT_TRUE(global_profile.has_value());
    EXPECT_EQ(
        *global_profile,
        make_line_profile("north", 6.0f, 7.0f, 1.4f, 0.75f)
    );

    const auto stream_profile = mgr.find_stream_line_profile("cam0", "north");
    ASSERT_TRUE(stream_profile.has_value());
    EXPECT_EQ(
        *stream_profile,
        make_line_profile("north", 2.0f, 2.0f, 0.75f, 0.25f)
    );
}

TEST(virtual_camera_test, latest_frame_replaces_previous_frame_for_stream) {
    virtual_camera camera("backend_cli");

    yodau::backend::frame first_frame;
    first_frame.width = 160;
    first_frame.height = 90;
    first_frame.stride = 160 * 3;
    first_frame.format = yodau::backend::pixel_format::bgr24;
    first_frame.data.resize(static_cast<size_t>(first_frame.stride) * 90u);

    yodau::backend::frame second_frame;
    second_frame.width = 640;
    second_frame.height = 360;
    second_frame.stride = 640 * 3;
    second_frame.format = yodau::backend::pixel_format::bgr24;
    second_frame.data.resize(static_cast<size_t>(second_frame.stride) * 360u);

    camera.publish("cam0", first_frame);
    camera.publish("cam0", second_frame);

    const auto latest = camera.latest_frame("cam0");
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest->width, 640);
    EXPECT_EQ(latest->height, 360);

    const std::vector<virtual_camera_frame_info> frames = camera.frames();
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames.front().stream_name, "cam0");
    EXPECT_EQ(frames.front().update_count, 2u);
    EXPECT_EQ(frames.front().bytes, second_frame.data.size());
}

TEST(virtual_camera_test, release_clears_cached_frame_state_for_stream) {
    virtual_camera camera("backend_cli");

    yodau::backend::frame frame_value;
    frame_value.width = 320;
    frame_value.height = 180;
    frame_value.stride = 320 * 3;
    frame_value.format = yodau::backend::pixel_format::bgr24;
    frame_value.data.resize(static_cast<size_t>(frame_value.stride) * 180u);

    camera.publish("cam0", frame_value);
    ASSERT_TRUE(camera.latest_frame("cam0").has_value());
    ASSERT_EQ(camera.frames().size(), 1u);

    camera.release("cam0");

    EXPECT_FALSE(camera.latest_frame("cam0").has_value());
    EXPECT_TRUE(camera.frames().empty());
}
