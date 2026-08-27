#include "configuration/line_configuration.hpp"

#include "analysis/processing_algorithm_catalog.hpp"
#include "analysis/processing_runtime.hpp"
#include "streams/stream_manager.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#ifdef __unix__
#include <sys/stat.h>
#endif

namespace {

yodau::core::line_configuration_document sample_configuration() {
    const auto& algorithms
        = yodau::core::default_processing_algorithm_descriptors();
    if (algorithms.empty()) {
        throw std::runtime_error("processing algorithm catalog is empty");
    }
    const auto& algorithm = algorithms.front();

    yodau::core::line_configuration_document document;
    document.stream.name = "front-door";
    document.stream.source = "/dev/video0";
    document.stream.type = "local";
    document.stream.loop = true;
    document.stream.virtual_camera_path = "/dev/yodau0";
    document.stream.analysis_interval_ms = 67;
    document.stream.algorithm
        = yodau::core::default_processing_algorithm_settings(algorithm.id);

    yodau::core::configured_line tripwire;
    tripwire.name = "entry";
    tripwire.points = { { 10.0F, 20.0F }, { 90.0F, 80.0F } };
    tripwire.direction = yodau::core::tripwire_dir::neg_to_pos;
    tripwire.enabled = true;
    tripwire.profile = yodau::core::make_line_profile(
        tripwire.name, 2.0F, 4.0F, 0.8F, 0.25F
    );
    tripwire.appearance.color = "#ff336699";
    tripwire.appearance.color_mode = "manual";
    tripwire.appearance.width_text = "2.0";
    tripwire.appearance.length_text = "0.8";
    tripwire.appearance.response_text = "0.25";
    document.lines.push_back(std::move(tripwire));
    return document;
}

std::filesystem::path unique_configuration_path() {
    const auto tick
        = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path()
        / ("yodau-line-configuration-" + std::to_string(tick) + ".json");
}

} // namespace

TEST(line_configuration_test, versioned_document_round_trips_every_setting) {
    const auto expected = sample_configuration();
    const std::string encoded
        = yodau::core::serialize_line_configuration(expected);
    const auto decoded = yodau::core::parse_line_configuration(encoded);

    EXPECT_EQ(decoded, expected);
    EXPECT_NE(encoded.find("yodau-line-configuration"), std::string::npos);
    EXPECT_NE(encoded.find("/dev/yodau0"), std::string::npos);
    EXPECT_NE(encoded.find("negative_to_positive"), std::string::npos);
}

TEST(line_configuration_test, rejects_unknown_fields_and_invalid_geometry) {
    const std::string unknown_field = R"({
        "format":"yodau-line-configuration",
        "version":1,
        "stream":{},
        "lines":[],
        "unexpected":true
    })";
    EXPECT_THROW(
        static_cast<void>(yodau::core::parse_line_configuration(unknown_field)),
        yodau::core::line_configuration_error
    );

    auto invalid = sample_configuration();
    invalid.lines.front().points = { { 10.0F, 10.0F }, { 10.0F, 10.0F } };
    EXPECT_THROW(
        static_cast<void>(yodau::core::serialize_line_configuration(invalid)),
        yodau::core::line_configuration_error
    );
}

TEST(line_configuration_test, atomic_file_round_trip_uses_shared_loader) {
    const auto expected = sample_configuration();
    const std::filesystem::path path = unique_configuration_path();
    std::error_code cleanup_error;

    yodau::core::save_line_configuration_atomic(expected, path);
    EXPECT_EQ(yodau::core::load_line_configuration(path), expected);

#ifdef __unix__
    struct stat status {};
    ASSERT_EQ(::stat(path.c_str(), &status), 0);
    EXPECT_EQ(status.st_mode & 0777, 0600);
#endif

    std::filesystem::remove(path, cleanup_error);
    EXPECT_FALSE(cleanup_error);
}

TEST(line_configuration_test, rejects_wrong_algorithm_parameter_types) {
    auto invalid = sample_configuration();
    const auto descriptor = yodau::core::processing_algorithm_descriptor_for(
        invalid.stream.algorithm.algorithm_id
    );
    ASSERT_TRUE(descriptor.has_value());
    ASSERT_FALSE(descriptor->parameters.empty());
    const auto& parameter = descriptor->parameters.front();
    invalid.stream.algorithm.parameter_overrides[parameter.id]
        = parameter.kind == yodau::core::processing_parameter_kind::boolean
        ? yodau::core::processing_algorithm_parameter_value { std::string(
              "not-a-boolean"
          ) }
        : yodau::core::processing_algorithm_parameter_value { true };

    EXPECT_THROW(
        static_cast<void>(yodau::core::serialize_line_configuration(invalid)),
        yodau::core::line_configuration_error
    );
}

TEST(line_configuration_test, rejects_noncanonical_names_and_unsafe_profiles) {
    auto invalid_name = sample_configuration();
    invalid_name.lines.front().name = " entry";
    invalid_name.lines.front().profile.line_name = " entry";
    EXPECT_THROW(
        static_cast<void>(
            yodau::core::serialize_line_configuration(invalid_name)
        ),
        yodau::core::line_configuration_error
    );

    auto invalid_profile = sample_configuration();
    invalid_profile.lines.front().profile.interaction_width = 101.0F;
    EXPECT_THROW(
        static_cast<void>(
            yodau::core::serialize_line_configuration(invalid_profile)
        ),
        yodau::core::line_configuration_error
    );
}

TEST(line_configuration_test, failed_atomic_save_preserves_existing_file) {
    const std::filesystem::path path = unique_configuration_path();
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(output.good());
        output << "existing configuration\n";
    }

    auto invalid = sample_configuration();
    invalid.lines.front().points = { { 10.0F, 10.0F } };
    EXPECT_THROW(
        yodau::core::save_line_configuration_atomic(invalid, path),
        yodau::core::line_configuration_error
    );

    std::ifstream input(path, std::ios::binary);
    ASSERT_TRUE(input.good());
    const std::string contents { std::istreambuf_iterator<char>(input),
                                 std::istreambuf_iterator<char>() };
    EXPECT_EQ(contents, "existing configuration\n");

    std::error_code cleanup_error;
    std::filesystem::remove(path, cleanup_error);
    EXPECT_FALSE(cleanup_error);
}

TEST(line_configuration_test, applies_lines_and_processing_to_core_runtime) {
    const auto document = sample_configuration();
    yodau::core::processing_runtime runtime(
        yodau::core::processing_runtime_options {
            .mode = yodau::core::render_mode::app_only,
            .enable_virtual_camera = false,
            .virtual_camera_device = {},
            .algorithm_id = document.stream.algorithm.algorithm_id,
        }
    );
    if (!runtime.processing_enabled()) {
        GTEST_SKIP() << "processing algorithms are disabled in this build";
    }
    yodau::core::stream_manager manager;
    runtime.attach(manager);

    const auto result
        = yodau::core::apply_line_configuration(document, manager, runtime);
    EXPECT_EQ(result.stream_name, document.stream.name);
    EXPECT_EQ(result.source, document.stream.source);
    EXPECT_EQ(result.virtual_camera_path, document.stream.virtual_camera_path);
    EXPECT_EQ(result.connected_line_count, 1U);
    ASSERT_NE(manager.find_stream(document.stream.name), nullptr);
    EXPECT_EQ(
        manager.stream_lines(document.stream.name),
        std::vector<std::string>({ "entry" })
    );
    const auto profile
        = manager.find_stream_line_profile(document.stream.name, "entry");
    ASSERT_TRUE(profile.has_value());
    EXPECT_FLOAT_EQ(profile->interaction_width, 4.0F);

    manager.shutdown();
    runtime.detach();
}

TEST(line_configuration_test, repeated_apply_preserves_disabled_line_settings) {
    auto document = sample_configuration();
    document.lines.front().enabled = false;
    document.stream.manual_processing_policy_enabled = true;
    document.stream.manual_processing_pixels = 640 * 480;
    yodau::core::processing_runtime runtime(
        yodau::core::processing_runtime_options {
            .mode = yodau::core::render_mode::app_only,
            .enable_virtual_camera = false,
            .virtual_camera_device = {},
            .algorithm_id = document.stream.algorithm.algorithm_id,
        }
    );
    if (!runtime.processing_enabled()) {
        GTEST_SKIP() << "processing algorithms are disabled in this build";
    }
    yodau::core::stream_manager manager;
    runtime.attach(manager);

    EXPECT_EQ(
        yodau::core::apply_line_configuration(document, manager, runtime)
            .connected_line_count,
        0U
    );
    EXPECT_EQ(
        yodau::core::apply_line_configuration(document, manager, runtime)
            .connected_line_count,
        0U
    );
    EXPECT_EQ(manager.line_names().size(), 1U);
    EXPECT_TRUE(manager.stream_lines(document.stream.name).empty());
    EXPECT_EQ(
        manager.find_stream_line_profile(document.stream.name, "entry"),
        document.lines.front().profile
    );
    EXPECT_EQ(
        runtime.stream_processing_max_pixels(document.stream.name),
        std::optional<int>(640 * 480)
    );

    manager.shutdown();
    runtime.detach();
}

TEST(
    line_configuration_test, shared_line_import_is_transactionally_preflighted
) {
    auto first = sample_configuration();
    first.stream.name = "camera-a";
    first.stream.source = "/tmp/camera-a.mp4";
    first.stream.type = "file";
    first.lines.front().profile.interaction_width = 4.0F;

    auto second = first;
    second.stream.name = "camera-b";
    second.stream.source = "/tmp/camera-b.mp4";
    second.lines.front().profile.interaction_width = 8.0F;

    yodau::core::processing_runtime runtime(
        yodau::core::processing_runtime_options {
            .mode = yodau::core::render_mode::app_only,
            .enable_virtual_camera = false,
            .virtual_camera_device = {},
            .algorithm_id = first.stream.algorithm.algorithm_id,
        }
    );
    if (!runtime.processing_enabled()) {
        GTEST_SKIP() << "processing algorithms are disabled in this build";
    }
    yodau::core::stream_manager manager;
    runtime.attach(manager);
    static_cast<void>(
        yodau::core::apply_line_configuration(first, manager, runtime)
    );
    static_cast<void>(
        yodau::core::apply_line_configuration(second, manager, runtime)
    );

    ASSERT_EQ(
        manager.find_stream_line_profile("camera-a", "entry")
            ->interaction_width,
        4.0F
    );
    ASSERT_EQ(
        manager.find_stream_line_profile("camera-b", "entry")
            ->interaction_width,
        8.0F
    );

    auto conflicting = second;
    conflicting.lines.front().points.back() = { 75.0F, 70.0F };
    EXPECT_THROW(
        static_cast<void>(
            yodau::core::apply_line_configuration(conflicting, manager, runtime)
        ),
        yodau::core::line_configuration_error
    );
    const auto retained_line = manager.find_line("entry");
    ASSERT_TRUE(retained_line);
    EXPECT_TRUE(retained_line->points.back().compare({ 90.0F, 80.0F }));
    EXPECT_FLOAT_EQ(
        manager.find_stream_line_profile("camera-a", "entry")
            ->interaction_width,
        4.0F
    );
    EXPECT_FLOAT_EQ(
        manager.find_stream_line_profile("camera-b", "entry")
            ->interaction_width,
        8.0F
    );

    manager.shutdown();
    runtime.detach();
}
