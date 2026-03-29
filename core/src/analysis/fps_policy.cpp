#include "analysis/fps_policy.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <thread>

namespace yodau::backend::fps_policy_support {

constexpr std::array<int, 10> interval_steps_ms { 25, 33,  41,  50,  66,
                                                  83, 100, 125, 166, 200 };

unsigned sanitized_hardware_threads(const unsigned hardware_threads) {
    return hardware_threads == 0 ? 1u : hardware_threads;
}

fps_capability_tier tier_for_threads(const unsigned hardware_threads) {
    if (hardware_threads <= 4u) {
        return fps_capability_tier::low;
    }

    if (hardware_threads <= 8u) {
        return fps_capability_tier::balanced;
    }

    return fps_capability_tier::high;
}

int snap_interval_ms(const double interval_ms) {
    const double clamped = std::clamp(
        interval_ms, static_cast<double>(interval_steps_ms.front()),
        static_cast<double>(interval_steps_ms.back())
    );
    int best = interval_steps_ms.front();
    double best_distance = std::abs(clamped - static_cast<double>(best));

    for (const int candidate : interval_steps_ms) {
        const double distance
            = std::abs(clamped - static_cast<double>(candidate));
        if (distance < best_distance) {
            best = candidate;
            best_distance = distance;
        }
    }

    return best;
}

int base_repaint_interval_ms(
    const fps_capability_tier tier, const fps_runtime_factors& factors
) {
    if (factors.mode == fps_mode::playback_only) {
        switch (tier) {
        case fps_capability_tier::low:
            return factors.role == fps_stream_role::active ? 50 : 66;
        case fps_capability_tier::balanced:
            return factors.role == fps_stream_role::active ? 33 : 41;
        case fps_capability_tier::high:
            return factors.role == fps_stream_role::active ? 25 : 33;
        }
    }

    switch (tier) {
    case fps_capability_tier::low:
        return factors.role == fps_stream_role::active ? 66 : 83;
    case fps_capability_tier::balanced:
        return factors.role == fps_stream_role::active ? 41 : 66;
    case fps_capability_tier::high:
        return factors.role == fps_stream_role::active ? 33 : 50;
    }

    return factors.role == fps_stream_role::active ? 41 : 66;
}

int base_analysis_interval_ms(
    const fps_capability_tier tier, const fps_runtime_factors& factors
) {
    if (factors.mode == fps_mode::playback_only) {
        return 0;
    }

    switch (tier) {
    case fps_capability_tier::low:
        return factors.role == fps_stream_role::active ? 100 : 125;
    case fps_capability_tier::balanced:
        return factors.role == fps_stream_role::active ? 66 : 83;
    case fps_capability_tier::high:
        return factors.role == fps_stream_role::active ? 50 : 66;
    }

    return factors.role == fps_stream_role::active ? 66 : 83;
}

int base_processing_scale_percent(
    const fps_capability_tier tier, const fps_runtime_factors& factors
) {
    if (factors.mode == fps_mode::playback_only) {
        return 100;
    }

    switch (tier) {
    case fps_capability_tier::low:
        return factors.role == fps_stream_role::active ? 80 : 60;
    case fps_capability_tier::balanced:
        return factors.role == fps_stream_role::active ? 90 : 70;
    case fps_capability_tier::high:
        return factors.role == fps_stream_role::active ? 100 : 82;
    }

    return factors.role == fps_stream_role::active ? 90 : 70;
}

double workload_units(const fps_runtime_factors& factors) {
    double units = 0.0;

    units
        += static_cast<double>(std::max(0, factors.configured_stream_count - 1))
        * 0.05;
    units += static_cast<double>(std::max(0, factors.visible_stream_count - 1))
        * 0.22;
    units
        += static_cast<double>(std::max(0, factors.grid_cell_count - 1)) * 0.06;
    units += static_cast<double>(std::max(0, factors.configured_line_count))
        * 0.04;
    units += static_cast<double>(std::max(0, factors.stream_line_count)) * 0.08;
    units += static_cast<double>(
                 std::min(std::max(0, factors.recent_motion_count), 24)
             )
        * 0.025;
    units += std::clamp(factors.device_load_ratio, 0.0, 2.5) * 0.90;

    if (factors.role == fps_stream_role::grid
        && factors.active_stream_count > 0) {
        units += 0.12;
    }

    if (factors.role == fps_stream_role::active) {
        units = std::max(0.0, units - 0.35);
    }

    return units;
}

} // namespace yodau::backend::fps_policy_support

std::string yodau::backend::fps_mode_name(const fps_mode mode) {
    switch (mode) {
    case fps_mode::playback_only:
        return "playback_only";
    case fps_mode::playback_and_processing:
        return "playback_and_processing";
    }

    return "playback_only";
}

std::string yodau::backend::fps_stream_role_name(const fps_stream_role role) {
    switch (role) {
    case fps_stream_role::grid:
        return "grid";
    case fps_stream_role::active:
        return "active";
    }

    return "grid";
}

std::string
yodau::backend::fps_capability_tier_name(const fps_capability_tier tier) {
    switch (tier) {
    case fps_capability_tier::low:
        return "low";
    case fps_capability_tier::balanced:
        return "balanced";
    case fps_capability_tier::high:
        return "high";
    }

    return "balanced";
}

yodau::backend::fps_capability_profile
yodau::backend::detect_fps_capability_profile() {
    const unsigned hardware_threads
        = fps_policy_support::sanitized_hardware_threads(
            std::thread::hardware_concurrency()
        );

    return fps_capability_profile {
        .hardware_threads = hardware_threads,
        .tier = fps_policy_support::tier_for_threads(hardware_threads),
    };
}

yodau::backend::fps_stream_profile yodau::backend::recommend_fps_profile(
    const fps_capability_profile& capability, const fps_runtime_factors& factors
) {
    const double workload = fps_policy_support::workload_units(factors);

    const int base_repaint = fps_policy_support::base_repaint_interval_ms(
        capability.tier, factors
    );
    const int base_analysis = fps_policy_support::base_analysis_interval_ms(
        capability.tier, factors
    );
    const int base_scale = fps_policy_support::base_processing_scale_percent(
        capability.tier, factors
    );

    const double repaint_factor = 1.0
        + workload * (factors.mode == fps_mode::playback_only ? 0.18 : 0.32);
    const double analysis_factor = 1.0 + workload * 0.55;

    fps_stream_profile profile;
    profile.repaint_interval_ms = fps_policy_support::snap_interval_ms(
        static_cast<double>(base_repaint) * repaint_factor
    );

    if (base_analysis > 0) {
        profile.analysis_interval_ms = fps_policy_support::snap_interval_ms(
            static_cast<double>(base_analysis) * analysis_factor
        );
    }

    const double quality_penalty
        = workload * (factors.role == fps_stream_role::active ? 4.0 : 7.0);
    const int min_quality = factors.role == fps_stream_role::active ? 70 : 45;
    profile.processing_scale_percent = std::clamp(
        base_scale - static_cast<int>(std::lround(quality_penalty)),
        min_quality, 100
    );

    if (factors.mode == fps_mode::playback_only) {
        profile.analysis_interval_ms = 0;
        profile.processing_scale_percent = 100;
    }

    return profile;
}
