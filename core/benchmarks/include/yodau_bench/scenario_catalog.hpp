#ifndef YODAU_BENCH_SCENARIO_CATALOG_HPP
#define YODAU_BENCH_SCENARIO_CATALOG_HPP

#include <string>
#include <vector>

namespace yodau::bench {

struct scenario_spec {
    std::string scenario_id;
    int stream_count { 1 };
    int lines_per_stream { 1 };
    int frame_count { 120 };
    int width { 640 };
    int height { 360 };
    int motion_objects { 1 };
    bool lighting_flicker { false };
    int camera_shake_px { 0 };
    bool crowded_motion { false };
};

std::vector<scenario_spec> stage0_scenarios();

} // namespace yodau::bench

#endif // YODAU_BENCH_SCENARIO_CATALOG_HPP
