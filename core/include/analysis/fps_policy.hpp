#ifndef YODAU_CORE_ANALYSIS_FPS_POLICY_HPP
#define YODAU_CORE_ANALYSIS_FPS_POLICY_HPP

#include <string>

namespace yodau::core {

enum class fps_mode { playback_only, playback_and_processing };

enum class fps_stream_role { grid, active };

enum class fps_capability_tier { low, balanced, high };

struct fps_capability_profile {
    unsigned hardware_threads { 1 };
    fps_capability_tier tier { fps_capability_tier::balanced };
};

struct fps_runtime_factors {
    fps_mode mode { fps_mode::playback_only };
    fps_stream_role role { fps_stream_role::grid };
    int configured_stream_count { 0 };
    int visible_stream_count { 0 };
    int active_stream_count { 0 };
    int configured_line_count { 0 };
    int stream_line_count { 0 };
    int grid_cell_count { 0 };
    int recent_motion_count { 0 };
    double device_load_ratio { 0.0 };
};

struct fps_stream_profile {
    int repaint_interval_ms { 66 };
    int analysis_interval_ms { 0 };
    int processing_scale_percent { 100 };
};

std::string fps_mode_name(fps_mode mode);
std::string fps_stream_role_name(fps_stream_role role);
std::string fps_capability_tier_name(fps_capability_tier tier);

fps_capability_profile detect_fps_capability_profile();

fps_stream_profile recommend_fps_profile(
    const fps_capability_profile& capability, const fps_runtime_factors& factors
);

} // namespace yodau::core

#endif // YODAU_CORE_ANALYSIS_FPS_POLICY_HPP
