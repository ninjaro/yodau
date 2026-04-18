#ifndef YODAU_CORE_ANALYSIS_PROCESSING_FRAME_TOOLS_HPP
#define YODAU_CORE_ANALYSIS_PROCESSING_FRAME_TOOLS_HPP

#ifdef YODAU_OPENCV

#include "analysis/processing_algorithm.hpp"
#include "streams/frame.hpp"

#include <opencv2/core/mat.hpp>

#include <string>

namespace yodau::core {

int config_int(
    const processing_algorithm_configuration& configuration,
    const std::string& key, int fallback, int min_value, int max_value
);

std::string config_string(
    const processing_algorithm_configuration& configuration,
    const std::string& key, std::string fallback
);

cv::Mat frame_to_gray_mat(const frame& frame_value);

point grid_cell_center_pct(int col, int row, int cols, int rows);

} // namespace yodau::core

#endif

#endif // YODAU_CORE_ANALYSIS_PROCESSING_FRAME_TOOLS_HPP
