#ifndef YODAU_CORE_ANALYSIS_PROCESSING_SPARSE_FLOW_HPP
#define YODAU_CORE_ANALYSIS_PROCESSING_SPARSE_FLOW_HPP

#ifdef YODAU_OPENCV

#include "analysis/processing_algorithm.hpp"

#include <opencv2/core/mat.hpp>

#include <cstddef>
#include <optional>
#include <vector>

namespace yodau::core {

struct processing_sparse_flow_options {
    int max_features { 80 };
    int quality_permille { 12 };
    int min_feature_distance_px { 7 };
    int window_size_px { 15 };
    int pyramid_levels { 3 };
    int max_error { 24 };
    int min_vector_length_px { 2 };
};

struct processing_sparse_flow_vector {
    point from_pct;
    point to_pct;
    double distance_pct { 0.0 };
    float error { 0.0f };
};

struct processing_sparse_flow_result {
    std::vector<processing_sparse_flow_vector> vectors;
    std::optional<point> average_from_pct;
    std::optional<point> average_to_pct;
    double average_distance_pct { 0.0 };
};

processing_sparse_flow_result sparse_optical_flow(
    const cv::Mat& previous_gray, const cv::Mat& current_gray,
    const cv::Mat& feature_mask,
    const processing_sparse_flow_options& options = {}
);

} // namespace yodau::core

#endif

#endif // YODAU_CORE_ANALYSIS_PROCESSING_SPARSE_FLOW_HPP
