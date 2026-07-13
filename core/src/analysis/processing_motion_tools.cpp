#include "analysis/processing_motion_tools.hpp"

#ifdef YODAU_OPENCV

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

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
    const cv::Mat diff
        = blurred_absdiff(previous_gray, current_gray, blur_kernel);

    cv::Mat binary_mask;
    cv::threshold(
        diff, binary_mask, static_cast<double>(diff_threshold), 255.0,
        cv::THRESH_BINARY
    );

    const int normalized_morph_kernel
        = normalized_odd_kernel_size(morph_kernel);
    if (normalized_morph_kernel > 1) {
        const cv::Mat kernel = cv::getStructuringElement(
            cv::MORPH_ELLIPSE,
            cv::Size(normalized_morph_kernel, normalized_morph_kernel)
        );
        cv::morphologyEx(binary_mask, binary_mask, cv::MORPH_CLOSE, kernel);
    }

    return binary_mask;
}

cv::Mat legacy_frame_delta_motion_mask(
    const cv::Mat& previous_gray, const cv::Mat& current_gray,
    const int diff_threshold, const int erode_iterations,
    const int dilate_iterations
) {
    cv::Mat mask;
    cv::absdiff(previous_gray, current_gray, mask);
    cv::threshold(
        mask, mask, static_cast<double>(diff_threshold), 255.0,
        cv::THRESH_BINARY
    );

    if (erode_iterations > 0) {
        cv::erode(mask, mask, cv::Mat(), cv::Point(-1, -1), erode_iterations);
    }
    if (dilate_iterations > 0) {
        cv::dilate(mask, mask, cv::Mat(), cv::Point(-1, -1), dilate_iterations);
    }

    return mask;
}

double motion_mask_ratio(const cv::Mat& binary_mask) {
    if (binary_mask.empty()) {
        return 0.0;
    }

    const int total = binary_mask.rows * binary_mask.cols;
    if (total <= 0) {
        return 0.0;
    }

    return static_cast<double>(cv::countNonZero(binary_mask))
        / static_cast<double>(total);
}

cv::Mat downsample_motion_mask_to_grid(
    const cv::Mat& binary_mask, const grid_dims& grid
) {
    cv::Mat out;
    if (grid.nx <= 0 || grid.ny <= 0 || binary_mask.empty()) {
        return out;
    }

    cv::resize(
        binary_mask, out, cv::Size(grid.nx, grid.ny), 0.0, 0.0, cv::INTER_AREA
    );
    return out;
}

std::vector<int>
active_motion_grid_cells(const cv::Mat& grid_mask, const grid_dims& grid) {
    std::vector<int> active_cell_indices;
    if (grid_mask.empty()) {
        return active_cell_indices;
    }

    active_cell_indices.reserve(
        static_cast<size_t>(grid.nx) * static_cast<size_t>(grid.ny)
    );
    for (int cell_y = 0; cell_y < grid.ny; ++cell_y) {
        const auto* row = grid_mask.ptr<std::uint8_t>(cell_y);
        for (int cell_x = 0; cell_x < grid.nx; ++cell_x) {
            if (row[cell_x] == 0) {
                continue;
            }

            active_cell_indices.push_back(cell_y * grid.nx + cell_x);
        }
    }

    return active_cell_indices;
}

double legacy_impact_speed(
    const point& previous_center, const point& current_center,
    const double motion_ratio, const double min_ratio
) {
    const auto dx = static_cast<double>(current_center.x - previous_center.x);
    const auto dy = static_cast<double>(current_center.y - previous_center.y);
    const double distance_pct = std::sqrt(dx * dx + dy * dy);
    const double ratio_boost
        = std::clamp((motion_ratio - min_ratio) * 18.0, 0.0, 1.2);
    return std::clamp(0.35 + distance_pct / 8.0 + ratio_boost, 0.35, 2.5);
}

event make_motion_event(
    std::string stream_name,
    const std::chrono::steady_clock::time_point timestamp,
    const point position_pct
) {
    event event_value;
    event_value.kind = event_kind::motion;
    event_value.stream_name = std::move(stream_name);
    event_value.ts = timestamp;
    event_value.pos_pct = position_pct;
    return event_value;
}

void append_motion_grid_cell_events(
    std::vector<event>& events, const std::string& stream_name,
    const std::chrono::steady_clock::time_point timestamp,
    const std::vector<int>& active_cell_indices, const grid_dims& grid,
    const int max_events
) {
    if (grid.nx <= 0 || grid.ny <= 0 || max_events <= 0) {
        return;
    }

    int emitted = 0;
    for (const int cell_idx : active_cell_indices) {
        const int cell_x = cell_idx % grid.nx;
        const int cell_y = cell_idx / grid.nx;

        events.push_back(make_motion_event(
            stream_name, timestamp,
            grid_cell_center_pct(grid_point { cell_x, cell_y }, grid)
        ));

        ++emitted;
        if (emitted >= max_events) {
            break;
        }
    }
}

bool processing_motion_event_state::update_previous_gray(
    const std::string& stream_name, const cv::Mat& current_gray,
    cv::Mat& previous_gray
) {
    std::scoped_lock lock(mtx_);
    auto it = previous_gray_by_stream_.find(stream_name);
    if (it == previous_gray_by_stream_.end()) {
        previous_gray_by_stream_.emplace(stream_name, current_gray.clone());
        return false;
    }

    previous_gray = it->second;
    if (previous_gray.empty() || previous_gray.size() != current_gray.size()
        || previous_gray.type() != current_gray.type()) {
        it->second = current_gray.clone();
        return false;
    }

    it->second = current_gray.clone();
    return true;
}

bool processing_motion_event_state::allow_motion_emit(
    const std::string& stream_name,
    const std::chrono::steady_clock::time_point now,
    const std::chrono::milliseconds cooldown
) {
    std::scoped_lock lock(mtx_);
    auto it = motion_emit_by_stream_.find(stream_name);
    if (it != motion_emit_by_stream_.end() && now - it->second < cooldown) {
        return false;
    }

    motion_emit_by_stream_[stream_name] = now;
    return true;
}

bool processing_motion_event_state::update_motion_position(
    const std::string& stream_name, const point current_position,
    point& previous_position
) {
    std::scoped_lock lock(mtx_);
    auto it = motion_position_by_stream_.find(stream_name);
    if (it == motion_position_by_stream_.end()) {
        motion_position_by_stream_[stream_name] = current_position;
        return false;
    }

    previous_position = it->second;
    it->second = current_position;
    return true;
}

bool processing_motion_event_state::allow_tripwire_emit(
    const std::string& key, const std::chrono::steady_clock::time_point now,
    const std::chrono::milliseconds cooldown
) {
    std::scoped_lock lock(mtx_);
    auto it = tripwire_emit_by_key_.find(key);
    if (it != tripwire_emit_by_key_.end() && now - it->second < cooldown) {
        return false;
    }

    tripwire_emit_by_key_[key] = now;
    return true;
}

} // namespace yodau::core

#endif
