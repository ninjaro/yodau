#include "daemon/headless_daemon.hpp"

#include "analysis/processing_algorithm_catalog.hpp"
#include "configuration/line_configuration_file.hpp"
#include "monitor/client.hpp"
#include "monitor/platform/shared_memory.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <sstream>
#include <stop_token>
#include <utility>

namespace {

std::filesystem::path unique_test_directory() {
    const auto tick
        = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path()
        / ("yodau-headless-test-" + std::to_string(tick));
}

yodau::core::line_configuration_document daemon_configuration(
    const std::filesystem::path& source,
    const std::filesystem::path& output_device
) {
    yodau::core::line_configuration_document document;
    document.stream.name = "daemon-test";
    document.stream.source = source.string();
    document.stream.type = "file";
    document.stream.loop = false;
    document.stream.virtual_camera_path = output_device.string();
    document.stream.analysis_interval_ms = 50;
    const auto& algorithms
        = yodau::core::default_processing_algorithm_descriptors();
    if (algorithms.empty()) {
        throw std::runtime_error("processing algorithm catalog is empty");
    }
    document.stream.algorithm
        = yodau::core::default_processing_algorithm_settings(
            algorithms.front().id
        );
    return document;
}

} // namespace

TEST(headless_daemon_test, requires_a_shared_line_configuration_file) {
    std::ostringstream output;
    std::ostringstream errors;

    const int exit_code = yodau::core::run_headless_daemon(
        {}, std::stop_token {}, output, errors
    );

    EXPECT_EQ(exit_code, 2);
    EXPECT_NE(
        errors.str().find("configuration file is required"), std::string::npos
    );
}

TEST(headless_daemon_test, capture_source_log_redacts_every_url_component) {
    EXPECT_EQ(
        yodau::core::redact_capture_source_for_log("/dev/video0"), "/dev/video0"
    );
    const std::string redacted = yodau::core::redact_capture_source_for_log(
        "rtsp://private-user:private-pass@example.test/private-token"
        "?access=secret#fragment-secret"
    );
    EXPECT_EQ(redacted, "rtsp://<redacted>");
    EXPECT_EQ(redacted.find("private"), std::string::npos);
    EXPECT_EQ(redacted.find("secret"), std::string::npos);
}

TEST(headless_daemon_test, rejects_malformed_configuration_before_capture) {
    const std::filesystem::path directory = unique_test_directory();
    ASSERT_TRUE(std::filesystem::create_directories(directory));
    const std::filesystem::path configuration_path = directory / "lines.json";
    {
        std::ofstream configuration(configuration_path);
        ASSERT_TRUE(configuration.good());
        configuration << R"({"format":"wrong"})";
    }

    std::ostringstream output;
    std::ostringstream errors;
    const int exit_code = yodau::core::run_headless_daemon(
        yodau::core::headless_daemon_options {
            .configuration_path = configuration_path,
            .source_override = std::nullopt,
            .output_device_override = std::nullopt,
            .stream_name_override = std::nullopt,
        },
        std::stop_token {}, output, errors
    );

    EXPECT_EQ(exit_code, 2);
    EXPECT_NE(
        errors.str().find("invalid line configuration"), std::string::npos
    );
    std::filesystem::remove_all(directory);
}

TEST(headless_daemon_test, rejects_source_output_alias_before_capture) {
    const std::filesystem::path directory = unique_test_directory();
    ASSERT_TRUE(std::filesystem::create_directories(directory));
    const std::filesystem::path source_path = directory / "source.raw";
    const std::filesystem::path output_alias = directory / "output-device";
    const std::filesystem::path configuration_path = directory / "lines.json";
    {
        std::ofstream source(source_path);
        ASSERT_TRUE(source.good());
        source << "not video";
    }
    std::filesystem::create_symlink(source_path, output_alias);
    yodau::core::save_line_configuration_file_atomic(
        daemon_configuration(source_path, output_alias), configuration_path
    );

    std::ostringstream output;
    std::ostringstream errors;
    const int exit_code = yodau::core::run_headless_daemon(
        yodau::core::headless_daemon_options {
            .configuration_path = configuration_path,
            .source_override = std::nullopt,
            .output_device_override = std::nullopt,
            .stream_name_override = std::nullopt,
        },
        std::stop_token {}, output, errors
    );

    EXPECT_EQ(exit_code, 1);
    EXPECT_NE(
        errors.str().find("must be different devices"), std::string::npos
    );
    std::filesystem::remove_all(directory);
}

#ifdef __linux__
TEST(headless_daemon_test, device_override_is_not_misclassified_as_a_file) {
    const std::filesystem::path directory = unique_test_directory();
    ASSERT_TRUE(std::filesystem::create_directories(directory));
    const std::filesystem::path source_path = directory / "source.raw";
    const std::filesystem::path output_path = directory / "output-device";
    const std::filesystem::path configuration_path = directory / "lines.json";
    {
        std::ofstream source(source_path);
        ASSERT_TRUE(source.good());
        source << "not video";
    }
    yodau::core::save_line_configuration_file_atomic(
        daemon_configuration(source_path, output_path), configuration_path
    );

    std::ostringstream output;
    std::ostringstream errors;
    const int exit_code = yodau::core::run_headless_daemon(
        yodau::core::headless_daemon_options {
            .configuration_path = configuration_path,
            .source_override = "/dev/null",
            .output_device_override = std::nullopt,
            .stream_name_override = std::nullopt,
        },
        std::stop_token {}, output, errors
    );

    EXPECT_EQ(exit_code, 1);
    EXPECT_NE(errors.str().find("V4L2"), std::string::npos);
    std::filesystem::remove_all(directory);
}

TEST(headless_daemon_test, configured_device_cannot_bypass_local_validation) {
    const std::filesystem::path directory = unique_test_directory();
    ASSERT_TRUE(std::filesystem::create_directories(directory));
    const std::filesystem::path output_path = directory / "output-device";
    const std::filesystem::path configuration_path = directory / "lines.json";
    yodau::core::save_line_configuration_file_atomic(
        daemon_configuration("/dev/null", output_path), configuration_path
    );

    std::ostringstream output;
    std::ostringstream errors;
    const int exit_code = yodau::core::run_headless_daemon(
        yodau::core::headless_daemon_options {
            .configuration_path = configuration_path,
            .source_override = std::nullopt,
            .output_device_override = std::nullopt,
            .stream_name_override = std::nullopt,
        },
        std::stop_token {}, output, errors
    );

    EXPECT_EQ(exit_code, 1);
    EXPECT_NE(errors.str().find("V4L2"), std::string::npos);
    std::filesystem::remove_all(directory);
}

TEST(headless_daemon_test, source_override_does_not_inherit_local_type) {
    const std::filesystem::path directory = unique_test_directory();
    ASSERT_TRUE(std::filesystem::create_directories(directory));
    const std::filesystem::path source_path = directory / "override.mp4";
    const std::filesystem::path output_path = directory / "output-device";
    const std::filesystem::path configuration_path = directory / "lines.json";
    {
        std::ofstream source(source_path);
        ASSERT_TRUE(source.good());
        source << "not video";
    }
    auto document = daemon_configuration("/dev/null", output_path);
    document.stream.type = "local";
    yodau::core::save_line_configuration_file_atomic(
        document, configuration_path
    );

    std::ostringstream output;
    std::ostringstream errors;
    const int exit_code = yodau::core::run_headless_daemon(
        yodau::core::headless_daemon_options {
            .configuration_path = configuration_path,
            .source_override = source_path.string(),
            .output_device_override = std::nullopt,
            .stream_name_override = std::nullopt,
        },
        std::stop_token {}, output, errors
    );

    EXPECT_EQ(exit_code, 1);
    EXPECT_EQ(errors.str().find("V4L2"), std::string::npos);
    EXPECT_NE(errors.str().find("capture"), std::string::npos);
    std::filesystem::remove_all(directory);
}
#endif

namespace {

class daemon_monitoring_test : public testing::Test {
protected:
    void SetUp() override {
        if constexpr (monitor::instrumentation_enabled) {
            ASSERT_TRUE(monitor::start_process("yodau-supervisor-test"));
            mapping = monitor::platform::open_process_mapping(
                monitor::platform::current_process_id()
            );
            ASSERT_TRUE(mapping.has_value());
            mapping->get()->watcher_lease_until_ns.store(
                std::numeric_limits<std::uint64_t>::max()
            );
            mapping->get()->watcher_present.store(1);
        }
    }

    void TearDown() override { monitor::client::process().stop(); }

    [[nodiscard]] monitor::heartbeat_slot& main_channel() const {
        return mapping->get()
            ->heartbeats[monitor::channel_index(monitor::channel::main)];
    }

    std::optional<monitor::platform::shared_mapping> mapping;
};

class inspected_errors : public std::stringbuf {
public:
    explicit inspected_errors(std::function<void()> inspect)
        : inspect_(std::move(inspect)) { }

protected:
    std::streamsize xsputn(const char* data, std::streamsize size) override {
        inspect_();
        return std::stringbuf::xsputn(data, size);
    }

private:
    std::function<void()> inspect_;
};

TEST_F(daemon_monitoring_test, startup_failure_retires_supervisor_expectation) {
    if constexpr (!monitor::instrumentation_enabled) {
        EXPECT_FALSE(monitor::process_available());
        return;
    }
    bool inspected = false;
    inspected_errors buffer([&] {
        inspected = true;
        EXPECT_EQ(main_channel().active.load(), 1U);
        EXPECT_GT(main_channel().expected_since_ns.load(), 0U);
        EXPECT_EQ(main_channel().sequence.load(), 0U);
    });
    std::ostream errors(&buffer);
    std::ostringstream output;
    // Existing malformed-input rejection supplies a deterministic startup exit,
    // before hardware capture or output is opened.
    const auto directory = unique_test_directory();
    ASSERT_TRUE(std::filesystem::create_directories(directory));
    yodau::core::headless_daemon_options options;
    options.configuration_path = directory / "absent.json";
    EXPECT_EQ(yodau::core::run_headless_daemon(options, {}, output, errors), 2);
    EXPECT_TRUE(inspected);
    EXPECT_EQ(main_channel().active.load(), 0U);
    EXPECT_EQ(main_channel().expected_since_ns.load(), 0U);
    std::filesystem::remove(directory);
}

TEST_F(
    daemon_monitoring_test, overlapping_supervisor_cannot_retire_existing_owner
) {
    if constexpr (!monitor::instrumentation_enabled) {
        return;
    }
    auto owner = monitor::expect_progress(monitor::channel::main);
    ASSERT_TRUE(owner.valid());
    const auto revision = main_channel().revision.load();
    const auto directory = unique_test_directory();
    ASSERT_TRUE(std::filesystem::create_directories(directory));
    yodau::core::headless_daemon_options options;
    options.configuration_path = directory / "absent.json";
    std::ostringstream output;
    std::ostringstream errors;
    EXPECT_EQ(yodau::core::run_headless_daemon(options, {}, output, errors), 2);
    EXPECT_NE(
        errors.str().find("overlapping headless supervisor"), std::string::npos
    );
    EXPECT_EQ(main_channel().active.load(), 1U);
    EXPECT_EQ(main_channel().revision.load(), revision);
    std::filesystem::remove(directory);
}

TEST_F(daemon_monitoring_test, empty_request_has_no_expected_supervision) {
    if constexpr (!monitor::instrumentation_enabled) {
        return;
    }
    const auto revision = main_channel().revision.load();
    std::ostringstream output;
    std::ostringstream errors;
    EXPECT_EQ(yodau::core::run_headless_daemon({}, {}, output, errors), 2);
    EXPECT_EQ(main_channel().active.load(), 0U);
    EXPECT_EQ(main_channel().revision.load(), revision);
}

TEST_F(
    daemon_monitoring_test, startup_activity_cannot_inherit_or_fake_progress
) {
    if constexpr (!monitor::instrumentation_enabled) {
        return;
    }
    {
        auto previous = monitor::expect_progress(monitor::channel::main);
        ASSERT_TRUE(previous.valid());
        previous.heartbeat();
        ASSERT_EQ(main_channel().sequence.load(), 1U);
    }
    const auto directory = unique_test_directory();
    ASSERT_TRUE(std::filesystem::create_directories(directory));
    yodau::core::headless_daemon_options options;
    options.configuration_path = directory / "absent.json";
    std::stop_source cancelled;
    cancelled.request_stop();
    for (const auto& stop : { std::stop_token {}, cancelled.get_token() }) {
        bool inspected = false;
        inspected_errors buffer([&] {
            inspected = true;
            const auto revision = main_channel().revision.load();
            const auto expected = main_channel().expected_since_ns.load();
            monitor::heartbeat(monitor::channel::main);
            monitor::record_breadcrumb(monitor::event::configuration_loaded);
            monitor::set_channel_active(monitor::channel::main, false);
            EXPECT_EQ(main_channel().active.load(), 1U);
            EXPECT_GT(expected, 0U);
            EXPECT_EQ(main_channel().revision.load(), revision);
            EXPECT_EQ(main_channel().sequence.load(), 0U);
            EXPECT_EQ(main_channel().timestamp_ns.load(), 0U);
            EXPECT_EQ(main_channel().expected_since_ns.load(), expected);
        });
        std::ostream errors(&buffer);
        std::ostringstream output;
        // Even a pre-requested stop retains the actual startup/unwind lifetime;
        // it does not silently change the host's configuration-error result.
        EXPECT_EQ(
            yodau::core::run_headless_daemon(options, stop, output, errors), 2
        );
        EXPECT_TRUE(inspected);
        EXPECT_EQ(main_channel().active.load(), 0U);
        EXPECT_EQ(main_channel().expected_since_ns.load(), 0U);
    }
    std::filesystem::remove(directory);
}

TEST_F(
    daemon_monitoring_test, cancelled_startup_does_not_announce_ready_output
) {
    if constexpr (!monitor::instrumentation_enabled) {
        return;
    }
    const auto directory = unique_test_directory();
    ASSERT_TRUE(std::filesystem::create_directories(directory));
    const auto configuration = directory / "lines.json";
    // Explicit cancellation skips the readiness wait; no capture/output device
    // or successful frame is needed to exercise the real startup/unwind path.
    yodau::core::save_line_configuration_file_atomic(
        daemon_configuration(
            directory / "absent.mp4", directory / "absent-output"
        ),
        configuration
    );
    yodau::core::headless_daemon_options options;
    options.configuration_path = configuration;
    std::stop_source cancelled;
    cancelled.request_stop();
    std::ostringstream output;
    std::ostringstream errors;
    EXPECT_EQ(
        yodau::core::run_headless_daemon(
            options, cancelled.get_token(), output, errors
        ),
        0
    );
    EXPECT_EQ(main_channel().active.load(), 0U);
    std::size_t ready_announcements = 0;
    for (const auto& slot : mapping->get()->breadcrumbs) {
        if (slot.commit.load() != 0
            && (slot.event_and_flags.load()
                & std::numeric_limits<std::uint16_t>::max())
                == static_cast<std::uint32_t>(monitor::event::output_ready)) {
            ++ready_announcements;
        }
    }
    EXPECT_EQ(ready_announcements, 0U);
    std::filesystem::remove_all(directory);
}

} // namespace
