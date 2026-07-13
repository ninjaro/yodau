#ifndef YODAU_CORE_ANALYSIS_PROCESSING_BACKGROUND_MODEL_HPP
#define YODAU_CORE_ANALYSIS_PROCESSING_BACKGROUND_MODEL_HPP

#ifdef YODAU_OPENCV

#include "core/namespace_alias.hpp"

#include <opencv2/core/mat.hpp>
#include <opencv2/video/background_segm.hpp>

#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace yodau::core {

enum class processing_background_model_kind { frame_delta, mog2, knn };

struct processing_background_model_options {
    processing_background_model_kind kind {
        processing_background_model_kind::frame_delta
    };
    int diff_threshold { 24 };
    int blur_kernel { 5 };
    int morph_kernel { 5 };
    int history_frames { 120 };
    double model_threshold { 16.0 };
    double learning_rate { 0.005 };
    bool detect_shadows { false };
};

processing_background_model_kind
background_model_kind_from_id(std::string_view value);
std::string
processing_background_model_kind_id(processing_background_model_kind kind);

class processing_background_model_store {
public:
    cv::Mat motion_mask(
        const std::string& stream_name, const cv::Mat& previous_gray,
        const cv::Mat& current_gray,
        const processing_background_model_options& options
    );

    void clear(const std::string& stream_name);

private:
    struct model_state {
        processing_background_model_kind kind {
            processing_background_model_kind::frame_delta
        };
        cv::Size frame_size;
        int history_frames { 120 };
        double model_threshold { 16.0 };
        bool detect_shadows { false };
        cv::Ptr<cv::BackgroundSubtractor> subtractor;
    };

    std::unordered_map<std::string, model_state> state_by_stream_;
    std::mutex mtx_;
};

} // namespace yodau::core

#endif

#endif // YODAU_CORE_ANALYSIS_PROCESSING_BACKGROUND_MODEL_HPP
