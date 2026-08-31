#ifndef YODAU_CORE_ANALYSIS_PROCESSING_MOTION_FOCUS_HPP
#define YODAU_CORE_ANALYSIS_PROCESSING_MOTION_FOCUS_HPP

#ifdef YODAU_OPENCV

#include "streams/stream.hpp"

#include <opencv2/core/mat.hpp>

#include <cstddef>
#include <string>
#include <string_view>

namespace yodau::core {

enum class processing_motion_focus_mode { off, auto_focus, regions, corridors };

struct processing_motion_focus_options {
    // Automatic mode preserves full-frame detection so downstream tripwire and
    // region filters retain observations and diagnostics. `regions` and
    // `corridors` opt into early spatial masking explicitly.
    processing_motion_focus_mode mode {
        processing_motion_focus_mode::auto_focus
    };
    float corridor_width_pct { 8.0f };
};

struct processing_motion_focus_result {
    cv::Mat mask;
    size_t shape_count { 0 };
};

processing_motion_focus_mode motion_focus_mode_from_id(std::string_view value);
std::string processing_motion_focus_mode_id(processing_motion_focus_mode mode);

processing_motion_focus_result build_motion_focus_mask(
    const stream& stream_value, cv::Size frame_size,
    const processing_motion_focus_options& options
);

} // namespace yodau::core

#endif

#endif // YODAU_CORE_ANALYSIS_PROCESSING_MOTION_FOCUS_HPP
