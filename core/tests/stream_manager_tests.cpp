#include "analysis/tripwire_grid_stream_index.hpp"
#include "geometry/coords.hpp"
#include "geometry/geometry.hpp"
#include "streams/stream_manager.hpp"
#include "streams/virtual_camera.hpp"

#include <gtest/gtest.h>

#include <functional>
#include <set>
#include <vector>

using yodau::backend::build_grid_stream_index;
using yodau::backend::collect_grid_candidates;
using yodau::backend::grid_candidate_tracker;
using yodau::backend::grid_dims;
using yodau::backend::grid_index;
using yodau::backend::line_ptr;
using yodau::backend::make_line;
using yodau::backend::point;
using yodau::backend::recommend_grid_dims;
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

} // namespace stream_manager_tests_support

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
