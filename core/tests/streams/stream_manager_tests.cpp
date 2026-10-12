#include "geometry/geometry.hpp"
#include "streams/analysis_scheduler.hpp"
#include "streams/stream.hpp"
#include "streams/stream_manager.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using yodau::core::analysis_scheduler;
using yodau::core::line_profile;
using yodau::core::line_ptr;
using yodau::core::make_line_profile;
using yodau::core::point;
using yodau::core::stream;
using yodau::core::stream_manager;

namespace stream_manager_tests_support {

struct processed_frame_probe {
    size_t call_count { 0 };
    std::string last_stream_name;
    int last_frame_width { 0 };
    size_t last_event_count { 0 };

    [[nodiscard]] static std::vector<yodau::core::event>
    process(const stream& stream_value, const yodau::core::frame& frame_value) {
        yodau::core::event event_value;
        event_value.kind = yodau::core::event_kind::motion;
        event_value.stream_name = stream_value.get_name();
        event_value.ts = frame_value.ts;
        event_value.pos_pct = point { 25.0f, 50.0f };
        return { event_value };
    }

    void on_processed_frame(
        const stream& stream_value, const yodau::core::frame& frame_value,
        const std::vector<yodau::core::event>& events
    ) {
        call_count += 1;
        last_stream_name = stream_value.get_name();
        last_frame_width = frame_value.width;
        last_event_count = events.size();
    }
};

} // namespace stream_manager_tests_support

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

TEST(stream_manager_test, processed_frame_sink_receives_frame_and_events) {
    stream_manager mgr;
    mgr.add_stream("/tmp/cam.mp4", "cam0", "file", true);

    stream_manager_tests_support::processed_frame_probe probe;
    mgr.set_frame_processor(
        &stream_manager_tests_support::processed_frame_probe::process
    );
    mgr.set_processed_frame_sink(
        std::bind_front(
            &stream_manager_tests_support::processed_frame_probe::
                on_processed_frame,
            &probe
        )
    );

    yodau::core::frame frame_value;
    frame_value.width = 320;
    frame_value.height = 180;
    frame_value.stride = 320 * 3;
    frame_value.format = yodau::core::pixel_format::rgb24;
    frame_value.data.resize(static_cast<size_t>(frame_value.stride) * 180u);
    frame_value.ts = std::chrono::steady_clock::now();

    mgr.push_frame("cam0", std::move(frame_value));

    EXPECT_EQ(probe.call_count, 1u);
    EXPECT_EQ(probe.last_stream_name, "cam0");
    EXPECT_EQ(probe.last_frame_width, 320);
    EXPECT_EQ(probe.last_event_count, 1u);
}

TEST(stream_manager_test, interval_override_and_clear) {
    stream_manager mgr;
    mgr.add_stream("/tmp/cam.mp4", "cam0", "file", true);
    mgr.set_analysis_interval_ms(1000);

    mgr.set_frame_processor(
        &stream_manager_tests_support::processed_frame_probe::process
    );

    yodau::core::frame frame_value;
    frame_value.ts = std::chrono::steady_clock::now();

    EXPECT_EQ(
        mgr.process_frame("cam0", yodau::core::frame(frame_value)).size(), 1u
    );
    EXPECT_TRUE(
        mgr.process_frame("cam0", yodau::core::frame(frame_value)).empty()
    );

    mgr.set_stream_analysis_interval_ms("cam0", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_EQ(
        mgr.process_frame("cam0", yodau::core::frame(frame_value)).size(), 1u
    );

    mgr.clear_stream_analysis_interval_ms("cam0");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_TRUE(
        mgr.process_frame("cam0", yodau::core::frame(frame_value)).empty()
    );
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

    mgr.set_line_profile(
        line_profile {
            .line_name = "north",
            .visual_width = 6.0f,
            .interaction_width = 0.0f,
            .effective_length = 2.5f,
            .damping = -1.0f,
        }
    );

    const auto stored_profile = mgr.find_line_profile("north");
    ASSERT_TRUE(stored_profile.has_value());
    EXPECT_EQ(
        *stored_profile, make_line_profile("north", 6.0f, 0.0f, 2.5f, -1.0f)
    );

    EXPECT_EQ(line_value->name, "north");
    EXPECT_EQ(line_value->points.size(), 2u);
    EXPECT_FALSE(line_value->closed);
}

TEST(stream_manager_test, stream_override_preserves_global_profile) {
    stream_manager mgr;

    mgr.add_stream("/tmp/cam.mp4", "cam0", "file", true);
    mgr.add_line("0,0;100,0", false, "north");
    mgr.set_line_profile(make_line_profile("north", 6.0f, 7.0f, 1.4f, 0.75f));
    mgr.set_line("cam0", "north");

    const auto inherited_profile
        = mgr.find_stream_line_profile("cam0", "north");
    ASSERT_TRUE(inherited_profile.has_value());
    EXPECT_EQ(
        *inherited_profile, make_line_profile("north", 6.0f, 7.0f, 1.4f, 0.75f)
    );

    mgr.set_stream_line_profile(
        "cam0", make_line_profile("north", 2.0f, 2.0f, 0.75f, 0.25f)
    );

    const auto global_profile = mgr.find_line_profile("north");
    ASSERT_TRUE(global_profile.has_value());
    EXPECT_EQ(
        *global_profile, make_line_profile("north", 6.0f, 7.0f, 1.4f, 0.75f)
    );

    const auto stream_profile = mgr.find_stream_line_profile("cam0", "north");
    ASSERT_TRUE(stream_profile.has_value());
    EXPECT_EQ(
        *stream_profile, make_line_profile("north", 2.0f, 2.0f, 0.75f, 0.25f)
    );
}

TEST(stream_manager_test, clear_stream_line_disconnects_only_selected_stream) {
    stream_manager mgr;

    mgr.add_stream("/tmp/cam0.mp4", "cam0", "file", true);
    mgr.add_stream("/tmp/cam1.mp4", "cam1", "file", true);
    mgr.add_line("0,0;100,0", false, "north");
    mgr.set_line("cam0", "north");
    mgr.set_line("cam1", "north");

    EXPECT_EQ(mgr.stream_lines("cam0").size(), 1u);
    EXPECT_EQ(mgr.stream_lines("cam1").size(), 1u);
    ASSERT_TRUE(mgr.find_stream_line_profile("cam0", "north").has_value());
    ASSERT_TRUE(mgr.find_stream_line_profile("cam1", "north").has_value());

    mgr.clear_stream_line("cam0", "north");

    EXPECT_TRUE(mgr.stream_lines("cam0").empty());
    EXPECT_EQ(mgr.stream_lines("cam1").size(), 1u);
    EXPECT_TRUE(mgr.find_stream_line_profile("cam0", "north").has_value());
    EXPECT_TRUE(mgr.find_stream_line_profile("cam1", "north").has_value());
    EXPECT_TRUE(mgr.find_line_profile("north").has_value());
}

TEST(stream_test, pipeline_state_supports_concurrent_status_reads) {
    stream value("/tmp/cam.mp4", "cam", "file", true);
    std::atomic<bool> invalid_value_seen { false };

    std::jthread writer([&value] {
        for (int index = 0; index < 20000; ++index) {
            value.activate(
                index % 2 == 0 ? yodau::core::stream_pipeline::manual
                               : yodau::core::stream_pipeline::automatic
            );
        }
        value.deactivate();
    });
    std::jthread reader([&value, &invalid_value_seen] {
        for (int index = 0; index < 20000; ++index) {
            const auto pipeline = value.pipeline();
            if (pipeline != yodau::core::stream_pipeline::manual
                && pipeline != yodau::core::stream_pipeline::automatic
                && pipeline != yodau::core::stream_pipeline::none) {
                invalid_value_seen.store(true);
            }
        }
    });

    writer.join();
    reader.join();
    EXPECT_FALSE(invalid_value_seen.load());
    EXPECT_EQ(value.pipeline(), yodau::core::stream_pipeline::none);
}
