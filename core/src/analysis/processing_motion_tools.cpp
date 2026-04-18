#include "analysis/processing_motion_tools.hpp"

#ifdef YODAU_OPENCV

#include <opencv2/imgproc.hpp>

#include <algorithm>

namespace yodau::core {

int normalized_odd_kernel_size(const int kernel_size) {
    int value = std::max(1, kernel_size);
    if (value % 2 == 0) {
        value += 1;
    }
    return value;
}

cv::Mat blurred_absdiff(
    const cv::Mat& previous_gray, const cv::Mat& current_gray,
    const int blur_kernel
) {
    cv::Mat diff;
    cv::absdiff(previous_gray, current_gray, diff);

    const int normalized_blur_kernel = normalized_odd_kernel_size(blur_kernel);
    if (normalized_blur_kernel > 1) {
        cv::GaussianBlur(
            diff, diff,
            cv::Size(normalized_blur_kernel, normalized_blur_kernel), 0.0
        );
    }

    return diff;
}

cv::Mat binary_motion_mask(
    const cv::Mat& previous_gray, const cv::Mat& current_gray,
    const int diff_threshold, const int blur_kernel, const int morph_kernel
) {
    const cv::Mat diff = blurred_absdiff(
        previous_gray, current_gray, blur_kernel
    );

    cv::Mat binary_mask;
    cv::threshold(
        diff, binary_mask, static_cast<double>(diff_threshold), 255.0,
        cv::THRESH_BINARY
    );

    const int normalized_morph_kernel = normalized_odd_kernel_size(morph_kernel);
    if (normalized_morph_kernel > 1) {
        const cv::Mat kernel = cv::getStructuringElement(
            cv::MORPH_ELLIPSE,
            cv::Size(normalized_morph_kernel, normalized_morph_kernel)
        );
        cv::morphologyEx(binary_mask, binary_mask, cv::MORPH_CLOSE, kernel);
    }

    return binary_mask;
}

} // namespace yodau::core

#endif
