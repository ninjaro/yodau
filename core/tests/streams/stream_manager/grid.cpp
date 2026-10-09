#include "analysis/opencv_client.hpp"
#include "analysis/tripwire_grid_stream_index.hpp"
#include "frames.hpp"
#include "geometry/coords.hpp"
#include "geometry/geometry.hpp"
#include "streams/stream.hpp"

#include <atomic>
#include <barrier>
#include <chrono>
#include <gtest/gtest.h>
#include <set>
#include <string>
#include <thread>
#include <vector>

using yodau::core::build_grid_stream_index;
using yodau::core::collect_grid_candidates;
using yodau::core::grid_candidate_tracker;
using yodau::core::grid_dims;
using yodau::core::grid_index;
using yodau::core::line_ptr;
using yodau::core::make_line;
using yodau::core::point;
using yodau::core::recommend_grid_dims;
using yodau::core::stream;
using yodau::core::trace_grid_cells_pct;

TEST(tripwire_grid_stream_index_test, grid_dims_focus_on_small_line) {
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

TEST(tripwire_grid_stream_index_test, candidates_stay_local_to_active_cells) {
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

#ifdef YODAU_OPENCV
TEST(opencv_grid_cache_test, snapshots_survive_parallel_stream_insertions) {
    constexpr int worker_count = 12;
    yodau::core::opencv_client client;
    std::barrier start_final_frame(worker_count);
    std::atomic<bool> failed { false };
    std::vector<std::jthread> workers;
    workers.reserve(worker_count);

    for (int index = 0; index < worker_count; ++index) {
        workers.emplace_back([&, index] {
            bool reached_barrier = false;
            try {
                stream source(
                    "/tmp/cache.mp4", "cache-" + std::to_string(index), "file",
                    true
                );
                source.connect_line(make_line(
                    { point { 50.0f, 5.0f }, point { 50.0f, 95.0f } }, "center"
                ));

                const auto timestamp = std::chrono::steady_clock::now();
                const auto dark = stream_manager_tests_support::make_gray_frame(
                    96, 96, timestamp
                );
                const auto bright
                    = stream_manager_tests_support::make_gray_frame(
                        96, 96, timestamp + std::chrono::milliseconds(200), 16,
                        24, 48, 72
                    );
                (void)client.motion_processor(source, dark);
                (void)client.motion_processor(source, bright);

                std::this_thread::sleep_for(std::chrono::milliseconds(170));
                start_final_frame.arrive_and_wait();
                reached_barrier = true;
                (void)client.motion_processor(source, dark);
            } catch (...) {
                if (!reached_barrier) {
                    start_final_frame.arrive_and_drop();
                }
                failed.store(true);
            }
        });
    }

    workers.clear();
    EXPECT_FALSE(failed.load());
}
#endif
