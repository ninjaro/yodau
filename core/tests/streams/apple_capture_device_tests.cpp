#include "analysis/processing_runtime.hpp"
#include "streams/apple_capture_device.hpp"
#include "streams/stream.hpp"
#include "streams/stream_manager.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <gtest/gtest.h>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using yodau::core::apple_capture_device_info;
using yodau::core::resolve_apple_capture_device;

namespace {

apple_capture_device_info
device(std::string unique_id, std::string name, bool connected = true) {
    apple_capture_device_info info;
    info.unique_id = std::move(unique_id);
    info.name = std::move(name);
    info.connected = connected;
    return info;
}

#ifdef __APPLE__
// Live tests open a real camera and may show the system permission prompt,
// so they only run when YODAU_LIVE_CAMERA=1.
bool live_camera_requested() {
    const char* value = std::getenv("YODAU_LIVE_CAMERA");
    return value != nullptr && std::string(value) == "1";
}

template <typename Predicate>
bool wait_until(Predicate predicate, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return predicate();
}
#endif

} // namespace

TEST(apple_capture_device_test, explicit_path_round_trips) {
    const std::string path
        = yodau::core::apple_capture_path_for_device_id("ABC-123");
    EXPECT_EQ(path, "avfoundation:ABC-123");
    EXPECT_EQ(
        yodau::core::apple_capture_device_id_from_path(path),
        std::optional<std::string>("ABC-123")
    );
    EXPECT_FALSE(
        yodau::core::apple_capture_device_id_from_path("avfoundation:")
    );
    EXPECT_FALSE(yodau::core::apple_capture_device_id_from_path("ABC-123"));
    EXPECT_EQ(
        yodau::core::stream::identify(path), yodau::core::stream_type::local
    );
}

TEST(apple_capture_device_test, stream_names_are_slugged_and_unique) {
    std::vector<std::string> taken;
    taken.push_back(
        yodau::core::apple_capture_stream_name("FaceTime HD Camera", taken)
    );
    taken.push_back(
        yodau::core::apple_capture_stream_name("FaceTime HD Camera", taken)
    );
    taken.push_back(yodau::core::apple_capture_stream_name("  ++ ", taken));
    taken.push_back(
        yodau::core::apple_capture_stream_name("iPhone (Desk View)", taken)
    );
    EXPECT_EQ(
        taken,
        (std::vector<std::string> { "facetime-hd-camera",
                                    "facetime-hd-camera-2", "camera",
                                    "iphone-desk-view" })
    );
}

TEST(apple_capture_device_test, resolves_explicit_bare_and_index_paths) {
    const std::vector<apple_capture_device_info> devices {
        device("BUILTIN", "FaceTime HD Camera"),
        device("UNPLUGGED", "USB Camera", false),
        device("PHONE", "iPhone Camera"),
    };

    EXPECT_EQ(
        resolve_apple_capture_device("avfoundation:PHONE", devices),
        std::optional<std::string>("PHONE")
    );
    EXPECT_EQ(
        resolve_apple_capture_device("BUILTIN", devices),
        std::optional<std::string>("BUILTIN")
    );
    // /dev/videoN counts connected devices only, as OpenCV's index did.
    EXPECT_EQ(
        resolve_apple_capture_device("/dev/video1", devices),
        std::optional<std::string>("PHONE")
    );
    EXPECT_FALSE(resolve_apple_capture_device("/dev/video2", devices));
    EXPECT_FALSE(resolve_apple_capture_device("/dev/video", devices));
    EXPECT_FALSE(resolve_apple_capture_device("/dev/video1x", devices));
    EXPECT_FALSE(resolve_apple_capture_device("UNPLUGGED", devices));
    EXPECT_FALSE(
        resolve_apple_capture_device("avfoundation:UNPLUGGED", devices)
    );
    EXPECT_FALSE(resolve_apple_capture_device("avfoundation:NOPE", devices));
    EXPECT_FALSE(resolve_apple_capture_device("/tmp/clip.mp4", devices));
}

TEST(apple_capture_device_test, selects_native_capture_for_local_streams_only) {
    const yodau::core::stream local_stream("BUILTIN", "camera", "local", false);
    const yodau::core::stream file_stream(
        "/tmp/clip.mp4", "clip", "file", false
    );
    EXPECT_EQ(
        yodau::core::uses_apple_capture(local_stream),
        yodau::core::apple_capture_available()
    );
    EXPECT_FALSE(yodau::core::uses_apple_capture(file_stream));
}

TEST(apple_capture_device_test, unknown_device_fails_before_any_capture) {
    std::stop_source stop;
    EXPECT_THROW(
        yodau::core::run_apple_capture(
            "avfoundation:yodau-test-missing-camera",
            [](yodau::core::frame&&) { FAIL() << "no frame expected"; },
            stop.get_token()
        ),
        std::runtime_error
    );
}

TEST(apple_capture_device_test, canonical_backend_is_unavailable) {
    // An Apple platform macro must not advertise a backend whose native
    // compilation/linkage is absent from the generated build contract.
    EXPECT_FALSE(yodau::core::apple_capture_available());
    EXPECT_TRUE(yodau::core::list_apple_capture_devices().empty());
    EXPECT_FALSE(resolve_apple_capture_device("avfoundation:BUILTIN"));
    EXPECT_EQ(
        yodau::core::apple_capture_authorization_status(),
        yodau::core::apple_capture_authorization::unavailable
    );
    EXPECT_EQ(
        yodau::core::request_apple_capture_authorization(
            std::chrono::milliseconds::zero()
        ),
        yodau::core::apple_capture_authorization::unavailable
    );
}

#ifndef __APPLE__
TEST(apple_capture_device_test, backend_is_absent_off_apple) {
    EXPECT_FALSE(yodau::core::apple_capture_available());
    EXPECT_TRUE(yodau::core::list_apple_capture_devices().empty());
    EXPECT_EQ(
        yodau::core::apple_capture_authorization_status(),
        yodau::core::apple_capture_authorization::unavailable
    );
}
#endif

#ifdef __APPLE__
TEST(apple_capture_device_test, discovery_reports_consistent_devices) {
    if (!yodau::core::apple_capture_available()) {
        GTEST_SKIP() << "native AVFoundation build integration is deferred";
    }
    const auto devices = yodau::core::list_apple_capture_devices();
    for (const auto& info : devices) {
        EXPECT_FALSE(info.unique_id.empty());
        EXPECT_FALSE(info.name.empty());
        for (const auto& format : info.formats) {
            EXPECT_GT(format.width, 0);
            EXPECT_GT(format.height, 0);
            EXPECT_LE(format.min_fps, format.max_fps);
            EXPECT_EQ(format.pixel_format.size(), 4U);
        }
    }

    yodau::core::stream_manager manager;
    std::size_t connected = 0;
    for (const auto& info : devices) {
        if (!info.connected) {
            continue;
        }
        ++connected;
        bool found = false;
        for (const std::string& name : manager.detected_local_stream_names()) {
            const auto detected = manager.find_stream(name);
            found
                = found || (detected && detected->get_path() == info.unique_id);
        }
        EXPECT_TRUE(found) << info.name;
    }
    EXPECT_GE(manager.detected_local_stream_names().size(), connected);
}

TEST(apple_capture_live_test, delivers_bgra_frames_until_stopped) {
    if (!yodau::core::apple_capture_available()) {
        GTEST_SKIP() << "native AVFoundation build integration is deferred";
    }
    if (!live_camera_requested()) {
        GTEST_SKIP() << "set YODAU_LIVE_CAMERA=1 to open a real camera";
    }
    const auto devices = yodau::core::list_apple_capture_devices();
    if (devices.empty()) {
        GTEST_SKIP() << "no camera connected";
    }

    std::stop_source stop;
    std::atomic<int> frames { 0 };
    std::mutex last_mtx;
    yodau::core::frame last;
    const auto started = std::chrono::steady_clock::now();
    std::thread worker([&] {
        yodau::core::run_apple_capture(
            devices.front().unique_id,
            [&](yodau::core::frame&& frame_value) {
                {
                    std::scoped_lock lock(last_mtx);
                    last = std::move(frame_value);
                }
                if (++frames >= 30) {
                    stop.request_stop();
                }
            },
            stop.get_token(),
            yodau::core::apple_capture_options { .preferred_width = 1280,
                                                 .preferred_height = 720 }
        );
    });
    worker.join();
    const auto elapsed = std::chrono::steady_clock::now() - started;

    EXPECT_GE(frames.load(), 30);
    std::scoped_lock lock(last_mtx);
    EXPECT_EQ(last.format, yodau::core::pixel_format::bgra32);
    EXPECT_TRUE(yodau::core::validate_frame_layout(last));
    EXPECT_EQ(last.width, 1280);
    EXPECT_EQ(last.height, 720);
    EXPECT_GE(last.stride, last.width * 4);
    EXPECT_LT(elapsed, std::chrono::seconds(10));
    std::cout << "[ live      ] " << devices.front().name << ": "
              << frames.load() << " frames " << last.width << "x" << last.height
              << " in "
              << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                     .count()
              << " ms\n";
}

TEST(apple_capture_live_test, stream_manager_runs_camera_without_qt) {
    if (!yodau::core::apple_capture_available()) {
        GTEST_SKIP() << "native AVFoundation build integration is deferred";
    }
    if (!live_camera_requested()) {
        GTEST_SKIP() << "set YODAU_LIVE_CAMERA=1 to open a real camera";
    }
    yodau::core::stream_manager manager;
    const auto names = manager.detected_local_stream_names();
    if (names.empty()) {
        GTEST_SKIP() << "no camera connected";
    }

    yodau::core::processing_runtime runtime(
        yodau::core::processing_runtime_options {
            .mode = yodau::core::render_mode::core_only,
            .enable_virtual_camera = false,
            .virtual_camera_device = {},
            .algorithm_id = {},
        }
    );
    std::atomic<int> processed { 0 };
    runtime.set_processed_frame_observer(
        [&](const yodau::core::stream&, const yodau::core::frame& frame_value,
            const yodau::core::processing_result&) {
            if (frame_value.format == yodau::core::pixel_format::bgra32) {
                ++processed;
            }
        }
    );
    runtime.attach(manager);

    const std::string& name = names.front();
    manager.start_stream(name);
    EXPECT_TRUE(manager.is_stream_running(name));
    EXPECT_TRUE(wait_until(
        [&] { return processed.load() >= 5; }, std::chrono::seconds(10)
    ));
    // A capture failure would end the daemon on its own with a failed status;
    // a requested stop leaves no status entry behind.
    const auto running_status = manager.stream_status(name);
    ASSERT_TRUE(running_status.has_value());
    EXPECT_EQ(running_status->state, yodau::core::stream_daemon_state::running)
        << running_status->error;
    manager.stop_stream(name);
    EXPECT_FALSE(manager.is_stream_running(name));
    EXPECT_FALSE(manager.stream_status(name).has_value());
    runtime.detach();
    manager.shutdown();
}
#endif
