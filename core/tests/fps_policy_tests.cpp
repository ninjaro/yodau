#include "analysis/fps_policy.hpp"

#include <gtest/gtest.h>

TEST(fps_policy_test, playback_only_disables_analysis_budgeting) {
    const yodau::backend::fps_capability_profile capability {
        .hardware_threads = 8,
        .tier = yodau::backend::fps_capability_tier::balanced,
    };
    const yodau::backend::fps_runtime_factors factors {
        .mode = yodau::backend::fps_mode::playback_only,
        .role = yodau::backend::fps_stream_role::grid,
        .configured_stream_count = 4,
        .visible_stream_count = 4,
        .active_stream_count = 1,
        .configured_line_count = 6,
        .stream_line_count = 2,
        .grid_cell_count = 4,
        .recent_motion_count = 3,
        .device_load_ratio = 0.8,
    };

    const auto profile
        = yodau::backend::recommend_fps_profile(capability, factors);

    EXPECT_EQ(profile.analysis_interval_ms, 0);
    EXPECT_EQ(profile.processing_scale_percent, 100);
    EXPECT_GT(profile.repaint_interval_ms, 0);
}

TEST(fps_policy_test, active_stream_keeps_higher_fps_and_quality) {
    const yodau::backend::fps_capability_profile capability {
        .hardware_threads = 8,
        .tier = yodau::backend::fps_capability_tier::balanced,
    };

    const yodau::backend::fps_runtime_factors grid_factors {
        .mode = yodau::backend::fps_mode::playback_and_processing,
        .role = yodau::backend::fps_stream_role::grid,
        .configured_stream_count = 4,
        .visible_stream_count = 4,
        .active_stream_count = 1,
        .configured_line_count = 8,
        .stream_line_count = 4,
        .grid_cell_count = 4,
        .recent_motion_count = 2,
        .device_load_ratio = 0.5,
    };
    auto active_factors = grid_factors;
    active_factors.role = yodau::backend::fps_stream_role::active;

    const auto grid_profile
        = yodau::backend::recommend_fps_profile(capability, grid_factors);
    const auto active_profile
        = yodau::backend::recommend_fps_profile(capability, active_factors);

    EXPECT_LE(
        active_profile.repaint_interval_ms, grid_profile.repaint_interval_ms
    );
    EXPECT_LE(
        active_profile.analysis_interval_ms, grid_profile.analysis_interval_ms
    );
    EXPECT_GE(
        active_profile.processing_scale_percent,
        grid_profile.processing_scale_percent
    );
}

TEST(fps_policy_test, heavier_runtime_pressure_reduces_budget) {
    const yodau::backend::fps_capability_profile capability {
        .hardware_threads = 8,
        .tier = yodau::backend::fps_capability_tier::balanced,
    };

    const yodau::backend::fps_runtime_factors light_factors {
        .mode = yodau::backend::fps_mode::playback_and_processing,
        .role = yodau::backend::fps_stream_role::grid,
        .configured_stream_count = 1,
        .visible_stream_count = 1,
        .active_stream_count = 0,
        .configured_line_count = 0,
        .stream_line_count = 0,
        .grid_cell_count = 1,
        .recent_motion_count = 0,
        .device_load_ratio = 0.0,
    };
    const yodau::backend::fps_runtime_factors heavy_factors {
        .mode = yodau::backend::fps_mode::playback_and_processing,
        .role = yodau::backend::fps_stream_role::grid,
        .configured_stream_count = 8,
        .visible_stream_count = 6,
        .active_stream_count = 1,
        .configured_line_count = 14,
        .stream_line_count = 6,
        .grid_cell_count = 9,
        .recent_motion_count = 12,
        .device_load_ratio = 1.8,
    };

    const auto light_profile
        = yodau::backend::recommend_fps_profile(capability, light_factors);
    const auto heavy_profile
        = yodau::backend::recommend_fps_profile(capability, heavy_factors);

    EXPECT_GE(
        heavy_profile.repaint_interval_ms, light_profile.repaint_interval_ms
    );
    EXPECT_GE(
        heavy_profile.analysis_interval_ms, light_profile.analysis_interval_ms
    );
    EXPECT_LE(
        heavy_profile.processing_scale_percent,
        light_profile.processing_scale_percent
    );
}
