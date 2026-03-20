#ifndef YODAU_BACKEND_OPENCV_CLIENT_HPP
#define YODAU_BACKEND_OPENCV_CLIENT_HPP

#ifdef YODAU_OPENCV

#include "analysis/tripwire_grid_stream_index.hpp"
#include "streams/event.hpp"
#include "streams/frame.hpp"
#include "streams/stream.hpp"
#include "streams/stream_manager.hpp"

#include <opencv2/opencv.hpp>

#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <unordered_map>
#include <vector>

namespace yodau::backend {

class opencv_client {
public:
    opencv_client() = default;

    void daemon_start(
        const stream& s, const std::function<void(frame&&)>& on_frame,
        const std::stop_token& st
    );

    std::vector<event> motion_processor(const stream& s, const frame& f);

    stream_manager::daemon_start_fn daemon_start_fn();
    stream_manager::frame_processor_fn frame_processor_fn();
    static opencv_client& shared_instance();

private:
    static cv::Mat
    downsample_to_grid_u8(const cv::Mat& diff, const grid_dims& g);
    static bool is_null_line_ptr(const line_ptr& line_ptr_value);
    static bool line_ptr_less_by_name(const line_ptr& a, const line_ptr& b);
    static void normalize_lines_snapshot(std::vector<line_ptr>& lines);

    int local_index_from_path(const std::string& path) const;
    frame mat_to_frame(const cv::Mat& m) const;

    float cross_z(const point& a, const point& b, const point& c) const;
    int orient(const point& a, const point& b, const point& c) const;
    bool between(float a, float b, float c) const;
    bool on_segment(const point& a, const point& b, const point& c) const;

    bool segments_intersect(
        const point& p1, const point& p2, const point& q1, const point& q2
    ) const;

    std::optional<point> segment_intersection(
        const point& p1, const point& p2, const point& q1, const point& q2
    ) const;

    void add_motion_event(
        std::vector<event>& out, const std::string& stream_name,
        const std::chrono::steady_clock::time_point ts, const point& pos_pct
    ) const;

    void consider_hit(
        bool& hit, float& best_dist2, point& best_a, point& best_b,
        point& best_pos, const point& cur_pos_pct, const point& a,
        const point& b, const point& pos
    ) const;

    void test_line_segment_against_contour(
        bool& hit, float& best_dist2, point& best_a, point& best_b,
        point& best_pos, const point& cur_pos_pct,
        const std::vector<point>& contour_pct, const point& a, const point& b,
        std::vector<point>& hit_positions_pct
    ) const;

    void process_tripwire_for_line(
        std::vector<event>& out, const stream& s, const line& l,
        const point& prev_pos, const point& cur_pos_pct,
        const std::vector<point>& contour_pct,
        const std::chrono::steady_clock::time_point now, double strength,
        const grid_line_index* line_index = nullptr,
        const std::vector<size_t>* candidate_segment_indices = nullptr
    );

    std::optional<size_t> find_largest_contour_index(
        const std::vector<std::vector<cv::Point>>& contours
    ) const;

    struct grid_cache_entry {
        grid_dims dims;
        std::vector<const line*> line_ptrs;
        grid_stream_index index;
        size_t reuse_count { 0 };
    };

    const grid_stream_index&
    get_grid_index_cached(const stream& s, const std::vector<line_ptr>& lines);

    mutable std::mutex grid_cache_mtx;
    std::unordered_map<std::string, grid_cache_entry> grid_cache_by_stream;
    mutable std::mutex mtx;

    std::unordered_map<std::string, cv::Mat> prev_gray_by_stream;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point>
        last_emit_by_stream;
    std::unordered_map<std::string, point> last_pos_by_stream;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point>
        last_tripwire_by_key;
};

void opencv_daemon_start(
    const stream& s, const std::function<void(frame&&)>& on_frame,
    const std::stop_token& st
);

std::vector<event> opencv_motion_processor(const stream& s, const frame& f);

}

#endif
#endif // YODAU_BACKEND_OPENCV_CLIENT_HPP
