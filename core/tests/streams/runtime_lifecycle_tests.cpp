#include "analysis/default_processing_hooks.hpp"
#include "analysis/processing_runtime.hpp"
#include "streams/linux_capture_device.hpp"
#include "streams/stream_daemon_runner.hpp"
#include "streams/virtual_camera.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <barrier>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <thread>

namespace {

template <typename Predicate> bool wait_until(Predicate predicate) {
    const auto deadline
        = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return predicate();
}

} // namespace

TEST(
    stream_daemon_runner_test, natural_completion_is_reported_and_restartable
) {
    yodau::core::stream_daemon_runner runner;
    auto stream_ptr = std::make_shared<yodau::core::stream>(
        "/tmp/source.mp4", "source", "file", false
    );
    const auto daemon = [](const yodau::core::stream&,
                           const std::function<void(yodau::core::frame&&)>&,
                           const std::stop_token&) { };
    const auto push = [](const std::string&, yodau::core::frame&&) { };

    ASSERT_TRUE(runner.start("source", stream_ptr, daemon, push));
    ASSERT_TRUE(wait_until([&runner] { return !runner.is_running("source"); }));
    const auto first_status = runner.status("source");
    ASSERT_TRUE(first_status.has_value());
    EXPECT_EQ(first_status->state, yodau::core::stream_daemon_state::completed);

    EXPECT_TRUE(runner.start("source", stream_ptr, daemon, push));
    EXPECT_TRUE(wait_until([&runner] { return !runner.is_running("source"); }));
}

TEST(stream_daemon_runner_test, exceptions_are_contained_and_reported) {
    yodau::core::stream_daemon_runner runner;
    auto stream_ptr = std::make_shared<yodau::core::stream>(
        "/tmp/source.mp4", "source", "file", false
    );

    ASSERT_TRUE(runner.start(
        "source", stream_ptr,
        [](const yodau::core::stream&,
           const std::function<void(yodau::core::frame&&)>&,
           const std::stop_token&) {
            throw std::runtime_error("capture failed");
        },
        [](const std::string&, yodau::core::frame&&) { }
    ));
    ASSERT_TRUE(wait_until([&runner] { return !runner.is_running("source"); }));

    const auto status = runner.status("source");
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(status->state, yodau::core::stream_daemon_state::failed);
    EXPECT_EQ(status->error, "capture failed");
}

TEST(stream_manager_test, naturally_completed_daemon_deactivates_and_restarts) {
    yodau::core::stream_manager manager;
    manager.add_stream("/tmp/source.mp4", "source", "file", false);
    manager.set_daemon_start_hook(
        [](const yodau::core::stream&,
           const std::function<void(yodau::core::frame&&)>&,
           const std::stop_token&) { }
    );

    manager.start_stream("source");
    ASSERT_TRUE(wait_until([&manager] {
        return !manager.is_stream_running("source");
    }));
    const auto status = manager.stream_status("source");
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(status->state, yodau::core::stream_daemon_state::completed);

    manager.start_stream("source");
    EXPECT_TRUE(wait_until([&manager] {
        return !manager.is_stream_running("source");
    }));
    EXPECT_TRUE(wait_until([&manager] {
        const auto source = manager.find_stream("source");
        return source
            && source->pipeline() == yodau::core::stream_pipeline::none;
    }));
}

TEST(virtual_camera_test, publication_is_bounded_when_output_is_unavailable) {
    yodau::core::virtual_camera camera(
        "test", "/dev/yodau-test-output-does-not-exist"
    );
    yodau::core::frame frame_value;
    frame_value.width = 64;
    frame_value.height = 48;
    frame_value.stride = frame_value.width * 3;
    frame_value.format = yodau::core::pixel_format::bgr24;
    frame_value.data.resize(
        static_cast<size_t>(frame_value.stride)
        * static_cast<size_t>(frame_value.height)
    );

    const auto started = std::chrono::steady_clock::now();
    for (size_t index = 0; index < 256U; ++index) {
        camera.publish("source", frame_value);
    }
    const auto elapsed = std::chrono::steady_clock::now() - started;
    EXPECT_LT(elapsed, std::chrono::seconds(5));
    ASSERT_TRUE(wait_until([&camera] {
        const auto frames = camera.frames();
        return !frames.empty() && !frames.front().last_error.empty();
    }));

    const auto frames = camera.frames();
    ASSERT_EQ(frames.size(), 1U);
    EXPECT_EQ(frames.front().update_count, 256U);
    EXPECT_GT(frames.front().dropped_frame_count, 0U);
    EXPECT_FALSE(frames.front().device_ready);
    camera.release("source");
    EXPECT_TRUE(camera.frames().empty());
}

TEST(virtual_camera_test, malformed_frame_is_rejected_before_retention) {
    yodau::core::virtual_camera camera(
        "test", "/dev/yodau-test-output-does-not-exist"
    );
    yodau::core::frame invalid;
    invalid.width = 64;
    invalid.height = 48;
    invalid.stride = 1;
    invalid.format = yodau::core::pixel_format::bgr24;
    invalid.data.resize(48U);

    camera.publish("source", invalid);

    EXPECT_FALSE(camera.latest_frame("source").has_value());
    const auto frames = camera.frames();
    ASSERT_EQ(frames.size(), 1U);
    EXPECT_EQ(frames.front().update_count, 0U);
    EXPECT_EQ(frames.front().dropped_frame_count, 1U);
    EXPECT_NE(
        frames.front().last_error.find("invalid source frame"),
        std::string::npos
    );
}

TEST(virtual_camera_test, explicit_output_is_not_shared_between_streams) {
    yodau::core::virtual_camera camera(
        "test", "/dev/yodau-test-output-does-not-exist"
    );
    yodau::core::frame frame_value;
    frame_value.width = 64;
    frame_value.height = 48;
    frame_value.stride = frame_value.width * 3;
    frame_value.format = yodau::core::pixel_format::bgr24;
    frame_value.data.resize(
        static_cast<size_t>(frame_value.stride)
        * static_cast<size_t>(frame_value.height)
    );

    camera.publish("first", frame_value);
    camera.publish("second", frame_value);
    ASSERT_TRUE(wait_until([&camera] {
        const auto frames = camera.frames();
        return frames.size() == 2U
            && std::ranges::any_of(frames, [](const auto& info) {
                   return info.last_error.find("already assigned")
                       != std::string::npos;
               });
    }));
}

TEST(
    processing_runtime_test, callbacks_become_inert_after_runtime_destruction
) {
    yodau::core::stream_manager::frame_processor_fn callback;
    {
        yodau::core::processing_runtime runtime(
            yodau::core::processing_runtime_options {
                .mode = yodau::core::render_mode::app_only,
                .enable_virtual_camera = false,
                .virtual_camera_device = {},
                .algorithm_id = yodau::core::default_processing_algorithm_id(),
            }
        );
        callback = runtime.frame_processor_hook();
    }

    if (!callback) {
        GTEST_SKIP() << "processing algorithms are disabled in this build";
    }
    const yodau::core::stream stream_value(
        "/tmp/source.mp4", "source", "file", false
    );
    EXPECT_TRUE(callback(stream_value, {}).empty());
}

TEST(processing_runtime_test, different_stream_callbacks_run_concurrently) {
    yodau::core::processing_runtime runtime(
        yodau::core::processing_runtime_options {
            .mode = yodau::core::render_mode::app_only,
            .enable_virtual_camera = false,
            .virtual_camera_device = {},
            .algorithm_id = yodau::core::default_processing_algorithm_id(),
        }
    );
    if (!runtime.processing_enabled()) {
        GTEST_SKIP() << "processing algorithms are disabled in this build";
    }
    yodau::core::stream_manager manager;
    manager.add_stream("/tmp/a.mp4", "a", "file", false);
    manager.add_stream("/tmp/b.mp4", "b", "file", false);
    runtime.attach(manager);

    std::atomic_int active_observers { 0 };
    std::atomic_int maximum_observers { 0 };
    runtime.set_processed_frame_observer(
        [&](const yodau::core::stream&, const yodau::core::frame&,
            const yodau::core::processing_result&) {
            const int active = active_observers.fetch_add(1) + 1;
            int observed_maximum = maximum_observers.load();
            while (active > observed_maximum
                   && !maximum_observers.compare_exchange_weak(
                       observed_maximum, active
                   )) { }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            active_observers.fetch_sub(1);
        }
    );

    const auto make_frame = [] {
        yodau::core::frame frame_value;
        frame_value.width = 64;
        frame_value.height = 48;
        frame_value.stride = frame_value.width * 3;
        frame_value.format = yodau::core::pixel_format::bgr24;
        frame_value.data.resize(
            static_cast<size_t>(frame_value.stride)
                * static_cast<size_t>(frame_value.height),
            0U
        );
        return frame_value;
    };

    std::barrier start_gate(3);
    std::jthread first([&] {
        start_gate.arrive_and_wait();
        manager.push_frame("a", make_frame());
    });
    std::jthread second([&] {
        start_gate.arrive_and_wait();
        manager.push_frame("b", make_frame());
    });
    start_gate.arrive_and_wait();
    first.join();
    second.join();

    EXPECT_GE(maximum_observers.load(), 2);
    manager.shutdown();
    runtime.detach();
}

TEST(processing_runtime_test, hot_unplug_releases_per_stream_state) {
    yodau::core::processing_runtime runtime(
        yodau::core::processing_runtime_options {
            .mode = yodau::core::render_mode::app_only,
            .enable_virtual_camera = false,
            .virtual_camera_device = {},
            .algorithm_id = yodau::core::default_processing_algorithm_id(),
        }
    );
    if (!runtime.processing_enabled()) {
        GTEST_SKIP() << "processing algorithms are disabled in this build";
    }
    yodau::core::stream_manager manager;
    bool camera_present = true;
    manager.set_local_stream_detector([&camera_present] {
        std::vector<yodau::core::stream> detected;
        if (camera_present) {
            detected.emplace_back("/tmp/fake-video2", "video2", "local", false);
        }
        return detected;
    });
    ASSERT_TRUE(manager.find_stream("video2"));
    runtime.attach(manager);
    runtime.set_stream_processing_max_pixels("video2", 640 * 480);

    const auto& algorithms
        = yodau::core::default_processing_algorithm_descriptors();
    ASSERT_FALSE(algorithms.empty());
    const std::string alternate_algorithm = algorithms.back().id;
    ASSERT_TRUE(runtime.set_stream_algorithm("video2", alternate_algorithm));

    camera_present = false;
    manager.refresh_local_streams();

    EXPECT_FALSE(manager.find_stream("video2"));
    EXPECT_FALSE(runtime.stream_processing_max_pixels("video2").has_value());
    EXPECT_FALSE(runtime.stream_algorithm_overrides().contains("video2"));
    manager.shutdown();
    runtime.detach();
}

#ifdef __linux__
TEST(
    linux_capture_device_test, candidate_discovery_keeps_sparse_numeric_nodes
) {
    const std::filesystem::path directory
        = std::filesystem::temp_directory_path()
        / ("yodau-capture-candidates-"
           + std::to_string(
               std::chrono::steady_clock::now().time_since_epoch().count()
           ));
    std::filesystem::create_directory(directory);
    {
        std::ofstream(directory / "video0").put('\n');
        std::ofstream(directory / "video2").put('\n');
        std::ofstream(directory / "video10").put('\n');
        std::ofstream(directory / "video-not-a-number").put('\n');
        std::ofstream(directory / "audio1").put('\n');
    }

    const auto candidates
        = yodau::core::list_linux_capture_device_candidates(directory);
    ASSERT_EQ(candidates.size(), 3U);
    EXPECT_EQ(std::filesystem::path(candidates[0]).filename(), "video0");
    EXPECT_EQ(std::filesystem::path(candidates[1]).filename(), "video2");
    EXPECT_EQ(std::filesystem::path(candidates[2]).filename(), "video10");

    std::filesystem::remove_all(directory);
}

TEST(
    linux_capture_device_test, exclusivity_probe_fails_closed_when_unverifiable
) {
    const auto result = yodau::core::probe_linux_capture_exclusivity(
        "/dev/yodau-definitely-missing-source"
    );
    EXPECT_EQ(
        result.state, yodau::core::linux_capture_exclusivity::indeterminate
    );
    EXPECT_FALSE(result.detail.empty());
}
#endif
