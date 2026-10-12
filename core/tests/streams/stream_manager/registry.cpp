#include "analysis/default_processing_hooks.hpp"
#include "analysis/processing_algorithm.hpp"
#include "analysis/processing_session_store.hpp"
#include "streams/stream.hpp"

#include <functional>
#include <gtest/gtest.h>
#include <memory>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

using yodau::core::default_processing_algorithm_id;
using yodau::core::default_processing_algorithm_registry;
using yodau::core::processing_algorithm;
using yodau::core::processing_algorithm_configuration;
using yodau::core::processing_algorithm_registry;
using yodau::core::processing_metric;
using yodau::core::processing_result;
using yodau::core::processing_session_store;
using yodau::core::stream;

namespace stream_manager_tests_support {

class dummy_processing_algorithm final : public processing_algorithm {
public:
    [[nodiscard]] std::string algorithm_id() const override { return "dummy"; }

    [[nodiscard]] std::string display_name() const override {
        return "dummy algorithm";
    }

    [[nodiscard]] processing_algorithm_configuration
    default_configuration() const override {
        processing_algorithm_configuration configuration;
        configuration.values.emplace("mode", "dummy");
        return configuration;
    }

    [[nodiscard]] processing_algorithm_configuration
    configuration() const override {
        return configuration_;
    }

    void configure(processing_algorithm_configuration configuration) override {
        configuration_ = std::move(configuration);
    }

    void daemon_start(
        const stream& stream_value,
        const std::function<void(yodau::core::frame&&)>& on_frame,
        const std::stop_token& stop_token
    ) override {
        (void)stream_value;
        (void)on_frame;
        (void)stop_token;
    }

    processing_result process_frame(
        const stream& stream_value, const yodau::core::frame& frame_value
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

class named_processing_algorithm final : public processing_algorithm {
public:
    explicit named_processing_algorithm(std::string algorithm_id_value)
        : algorithm_id_value_(std::move(algorithm_id_value)) { }

    [[nodiscard]] std::string algorithm_id() const override {
        return algorithm_id_value_;
    }

    [[nodiscard]] std::string display_name() const override {
        return algorithm_id_value_ + " algorithm";
    }

    void daemon_start(
        const stream& stream_value,
        const std::function<void(yodau::core::frame&&)>& on_frame,
        const std::stop_token& stop_token
    ) override {
        (void)stream_value;
        (void)on_frame;
        (void)stop_token;
    }

    processing_result process_frame(
        const stream& stream_value, const yodau::core::frame& frame_value
    ) override {
        (void)stream_value;
        (void)frame_value;
        return {};
    }

private:
    std::string algorithm_id_value_;
};

} // namespace stream_manager_tests_support

TEST(processing_algorithm_registry_test, registry_creates_named_algorithm) {
    processing_algorithm_registry registry;

    EXPECT_TRUE(registry.register_algorithm(
        processing_algorithm_registry::entry {
            .algorithm_id = "dummy",
            .display_name = "dummy algorithm",
            .create =
                [] {
                    return std::make_unique<stream_manager_tests_support::
                                                dummy_processing_algorithm>();
                },
        }
    ));
    EXPECT_FALSE(registry.register_algorithm(
        processing_algorithm_registry::entry {
            .algorithm_id = "dummy",
            .display_name = "dummy duplicate",
            .create =
                [] {
                    return std::make_unique<stream_manager_tests_support::
                                                dummy_processing_algorithm>();
                },
        }
    ));

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
        stream("/tmp/cam.mp4", "cam0", "file", true), yodau::core::frame {}
    );
    ASSERT_EQ(result.metrics.size(), 1u);
    EXPECT_EQ(result.metrics.front().name, "stream_name_size");
    EXPECT_EQ(result.metrics.front().unit, "chars");
    EXPECT_DOUBLE_EQ(result.metrics.front().value, 4.0);
}

TEST(processing_session_store_test, session_overrides_cache_and_results) {
    processing_session_store store("spot_grid");

    const processing_session_store::legacy_algorithm_factory factory
        = [](const std::string& algorithm_id) {
              return std::make_unique<
                  stream_manager_tests_support::named_processing_algorithm>(
                  algorithm_id
              );
          };

    EXPECT_TRUE(store.processing_enabled());
    EXPECT_EQ(store.default_algorithm_id(), "spot_grid");
    EXPECT_EQ(store.algorithm_id_for_stream("cam-a"), "spot_grid");

    auto default_algorithm
        = store.active_algorithm_for_stream("cam-a", factory);
    ASSERT_TRUE(default_algorithm);
    EXPECT_EQ(default_algorithm->algorithm_id(), "spot_grid");

    processing_result default_result;
    default_result.diagnostics.push_back({ .key = "stream", .value = "cam-a" });
    store.store_latest_processing_result("cam-a", default_result);
    ASSERT_TRUE(store.latest_processing_result("cam-a").has_value());

    auto override_algorithm = std::shared_ptr<processing_algorithm>(
        std::make_unique<
            stream_manager_tests_support::named_processing_algorithm>(
            "contour_mask"
        )
    );
    store.set_stream_algorithm("cam-b", "contour_mask", override_algorithm);
    EXPECT_EQ(store.algorithm_id_for_stream("cam-b"), "contour_mask");
    ASSERT_EQ(store.stream_algorithm_overrides().size(), 1u);
    EXPECT_EQ(store.stream_algorithm_overrides().at("cam-b"), "contour_mask");

    processing_result override_result;
    override_result.diagnostics.push_back(
        { .key = "stream", .value = "cam-b" }
    );
    store.store_latest_processing_result("cam-b", override_result);
    ASSERT_TRUE(store.latest_processing_result("cam-b").has_value());

    store.set_default_algorithm("motion_baseline");
    EXPECT_EQ(store.default_algorithm_id(), "motion_baseline");
    EXPECT_EQ(store.algorithm_id_for_stream("cam-a"), "motion_baseline");
    EXPECT_FALSE(store.latest_processing_result("cam-a").has_value());
    ASSERT_TRUE(store.latest_processing_result("cam-b").has_value());

    auto refreshed_default_algorithm
        = store.active_algorithm_for_stream("cam-a", factory);
    ASSERT_TRUE(refreshed_default_algorithm);
    EXPECT_EQ(refreshed_default_algorithm->algorithm_id(), "motion_baseline");
    EXPECT_NE(refreshed_default_algorithm, default_algorithm);

    store.clear_stream_algorithm("cam-b");
    EXPECT_EQ(store.algorithm_id_for_stream("cam-b"), "motion_baseline");
    EXPECT_TRUE(store.stream_algorithm_overrides().empty());
    EXPECT_FALSE(store.latest_processing_result("cam-b").has_value());
}

#ifdef YODAU_OPENCV
TEST(
    processing_algorithm_registry_test, default_registry_exposes_motion_baseline
) {
    const auto& registry = default_processing_algorithm_registry();

    EXPECT_TRUE(registry.contains(default_processing_algorithm_id()));
    EXPECT_TRUE(registry.contains("spot_grid"));
    EXPECT_TRUE(registry.contains("spot grid"));
    EXPECT_TRUE(registry.contains("contour_mask"));
    EXPECT_TRUE(registry.contains("contour mask"));
    EXPECT_TRUE(registry.contains("centroid_track"));
    EXPECT_TRUE(registry.contains("centroid track"));
    EXPECT_TRUE(registry.contains("hybrid_auto"));

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

    auto centroid_track_algorithm
        = yodau::core::make_processing_algorithm("track");
    ASSERT_TRUE(centroid_track_algorithm);
    EXPECT_EQ(centroid_track_algorithm->algorithm_id(), "centroid_track");
    EXPECT_EQ(centroid_track_algorithm->display_name(), "centroid track");

    auto hybrid_auto_algorithm
        = yodau::core::make_processing_algorithm("hybrid");
    ASSERT_TRUE(hybrid_auto_algorithm);
    EXPECT_EQ(hybrid_auto_algorithm->algorithm_id(), "hybrid_auto");
    EXPECT_EQ(hybrid_auto_algorithm->display_name(), "hybrid auto");
}
#endif
