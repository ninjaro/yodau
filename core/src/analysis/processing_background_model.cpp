#include "analysis/processing_background_model.hpp"

#ifdef YODAU_OPENCV

#include "analysis/processing_motion_tools.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cctype>
#include <utility>

namespace yodau::core {

namespace {

std::string normalized_model_id(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());

    for (const char ch : value) {
        if (std::isspace(static_cast<unsigned char>(ch)) || ch == '-') {
            if (!normalized.empty() && normalized.back() != '_') {
                normalized.push_back('_');
            }
            continue;
        }

        normalized.push_back(
            static_cast<char>(std::tolower(static_cast<unsigned char>(ch)))
        );
    }

    while (!normalized.empty() && normalized.back() == '_') {
        normalized.pop_back();
    }

    return normalized;
}

cv::Mat blurred_gray(const cv::Mat& gray, const int blur_kernel) {
    const int kernel_size = normalized_odd_kernel_size(blur_kernel);
    if (kernel_size <= 1) {
        return gray;
    }

    cv::Mat blurred;
    cv::GaussianBlur(gray, blurred, cv::Size(kernel_size, kernel_size), 0.0);
    return blurred;
}

cv::Ptr<cv::BackgroundSubtractor> make_subtractor(
    const processing_background_model_kind kind, const int history_frames,
    const double model_threshold, const bool detect_shadows
) {
    if (kind == processing_background_model_kind::knn) {
        return cv::createBackgroundSubtractorKNN(
            history_frames, model_threshold, detect_shadows
        );
    }

    return cv::createBackgroundSubtractorMOG2(
        history_frames, model_threshold, detect_shadows
    );
}

} // namespace

processing_background_model_kind processing_background_model_kind_from_id(
    std::string_view value
) {
    const std::string normalized = normalized_model_id(value);
    if (normalized == "mog2" || normalized == "gaussian_mixture") {
        return processing_background_model_kind::mog2;
    }
    if (normalized == "knn" || normalized == "nearest_neighbors") {
        return processing_background_model_kind::knn;
    }
    return processing_background_model_kind::frame_delta;
}

std::string processing_background_model_kind_id(
    const processing_background_model_kind kind
) {
    if (kind == processing_background_model_kind::mog2) {
        return "mog2";
    }
    if (kind == processing_background_model_kind::knn) {
        return "knn";
    }
    return "frame_delta";
}

cv::Mat processing_background_model_store::motion_mask(
    const std::string& stream_name, const cv::Mat& previous_gray,
    const cv::Mat& current_gray,
    const processing_background_model_options& options
) {
    if (previous_gray.empty() || current_gray.empty()
        || previous_gray.size() != current_gray.size()) {
        return {};
    }

    if (options.kind == processing_background_model_kind::frame_delta) {
        return binary_motion_mask(
            previous_gray, current_gray, options.diff_threshold,
            options.blur_kernel, options.morph_kernel
        );
    }

    const int history_frames = std::max(2, options.history_frames);
    const double model_threshold = std::max(1.0, options.model_threshold);
    const double learning_rate = std::clamp(options.learning_rate, -1.0, 1.0);

    const cv::Mat previous_input = blurred_gray(previous_gray, options.blur_kernel);
    const cv::Mat current_input = blurred_gray(current_gray, options.blur_kernel);

    std::scoped_lock lock(mtx_);
    model_state& state = state_by_stream_[stream_name];
    processing_background_model_options normalized_options = options;
    normalized_options.history_frames = history_frames;
    normalized_options.model_threshold = model_threshold;

    const bool state_matches = state.subtractor != nullptr
        && state.kind == normalized_options.kind
        && state.frame_size == current_input.size()
        && state.history_frames == normalized_options.history_frames
        && state.model_threshold == normalized_options.model_threshold
        && state.detect_shadows == normalized_options.detect_shadows;
    if (!state_matches) {
        state.kind = options.kind;
        state.frame_size = current_input.size();
        state.history_frames = history_frames;
        state.model_threshold = model_threshold;
        state.detect_shadows = options.detect_shadows;
        state.subtractor = make_subtractor(
            options.kind, history_frames, model_threshold,
            options.detect_shadows
        );

        cv::Mat ignored;
        state.subtractor->apply(previous_input, ignored, 1.0);
    }

    cv::Mat foreground;
    state.subtractor->apply(current_input, foreground, learning_rate);

    cv::Mat binary_mask;
    cv::threshold(foreground, binary_mask, 200.0, 255.0, cv::THRESH_BINARY);

    const int morph_kernel = normalized_odd_kernel_size(options.morph_kernel);
    if (morph_kernel > 1) {
        const cv::Mat kernel = cv::getStructuringElement(
            cv::MORPH_ELLIPSE, cv::Size(morph_kernel, morph_kernel)
        );
        cv::morphologyEx(binary_mask, binary_mask, cv::MORPH_CLOSE, kernel);
    }

    return binary_mask;
}

void processing_background_model_store::clear(const std::string& stream_name) {
    std::scoped_lock lock(mtx_);
    state_by_stream_.erase(stream_name);
}

} // namespace yodau::core

#endif
