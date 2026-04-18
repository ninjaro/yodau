#ifndef YODAU_CORE_ANALYSIS_PROCESSING_MOTION_TOOLS_HPP
#define YODAU_CORE_ANALYSIS_PROCESSING_MOTION_TOOLS_HPP

#ifdef YODAU_OPENCV

#include "core/namespace_alias.hpp"

#include <opencv2/core/mat.hpp>

namespace yodau::core {

int normalized_odd_kernel_size(int kernel_size);

cv::Mat blurred_absdiff(
    const cv::Mat& previous_gray, const cv::Mat& current_gray, int blur_kernel
);

cv::Mat binary_motion_mask(
    const cv::Mat& previous_gray, const cv::Mat& current_gray,
    int diff_threshold, int blur_kernel, int morph_kernel
);

} // namespace yodau::core

#endif

#endif // YODAU_CORE_ANALYSIS_PROCESSING_MOTION_TOOLS_HPP
