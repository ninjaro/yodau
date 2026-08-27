#include "daemon/headless_daemon.hpp"

#include "analysis/processing_algorithm_catalog.hpp"
#include "configuration/line_configuration.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stop_token>

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
        configuration << R"({"format":"wrong","version":1})";
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
    yodau::core::save_line_configuration_atomic(
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
    yodau::core::save_line_configuration_atomic(
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
    yodau::core::save_line_configuration_atomic(
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
    yodau::core::save_line_configuration_atomic(document, configuration_path);

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
