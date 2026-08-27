#ifdef YODAU_OPENCV

#include "analysis/opencv_client.hpp"
#include "analysis/processing_contour_tools.hpp"
#include "analysis/processing_frame_tools.hpp"
#include "analysis/processing_motion_tools.hpp"
#include "analysis/processing_tripwire_tools.hpp"
#include "analysis/tripwire_grid_stream_index.hpp"
#include "geometry/coords.hpp"

#include <algorithm>
#include <cmath>
#include <opencv2/core/version.hpp>
#include <optional>
#if CV_VERSION_MAJOR >= 5
#include <opencv2/geometry.hpp>
#endif
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <stdexcept>

// #define YODAU_DUMP_DEBUG_FRAMES
// #define YODAU_DEBUG_GRID
#define YODAU_GRID_PREFILTER

#ifdef YODAU_DUMP_DEBUG_FRAMES
#include <opencv2/imgcodecs.hpp>
#endif

namespace yodau::core {

namespace opencv_client_support {

    constexpr size_t grid_layout_recalc_interval = 120;

    bool same_line_ptrs(
        const std::vector<line_ptr>& lhs, const std::vector<line_ptr>& rhs
    ) {
        if (lhs.size() != rhs.size()) {
            return false;
        }

        for (size_t i = 0; i < lhs.size(); ++i) {
            if (lhs[i].get() != rhs[i].get()) {
                return false;
            }
        }

        return true;
    }

    const line* find_focus_line_for_rebuild(
        const std::vector<line_ptr>& previous_lines,
        const std::vector<line_ptr>& current_lines
    ) {
        const line* focus_line = nullptr;

        for (const auto& line_ptr_value : current_lines) {
            if (!line_ptr_value) {
                continue;
            }

            bool seen_before = false;
            for (const auto& previous_line : previous_lines) {
                if (previous_line.get() == line_ptr_value.get()) {
                    seen_before = true;
                    break;
                }
            }

            if (!seen_before) {
                focus_line = line_ptr_value.get();
            }
        }

        return focus_line;
    }

} // namespace opencv_client_support

bool opencv_client::is_null_line_ptr(const line_ptr& line_ptr_value) {
    return !line_ptr_value;
}

bool opencv_client::line_ptr_less_by_name(
    const line_ptr& a, const line_ptr& b
) {
    const bool a_ok = static_cast<bool>(a);
    const bool b_ok = static_cast<bool>(b);

    if (!b_ok) {
        return false;
    }
    if (!a_ok) {
        return true;
    }

    if (a->name < b->name) {
        return true;
    }
    if (b->name < a->name) {
        return false;
    }

    return a.get() < b.get();
}

void opencv_client::normalize_lines_snapshot(std::vector<line_ptr>& lines) {
    std::erase_if(lines, opencv_client::is_null_line_ptr);
    std::sort(lines.begin(), lines.end(), opencv_client::line_ptr_less_by_name);
}

void opencv_client::daemon_start(
    const stream& s, const std::function<void(frame&&)>& on_frame,
    const stop_token& st
) {
    cv::VideoCapture cap;
    if (!open_video_capture_for_stream(s, cap)) {
        throw std::runtime_error(
            "capture source could not be opened (stream type: "
            + stream::type_name(s.get_type()) + ")"
        );
    }

    cv::Mat m;
    while (!st.stop_requested()) {
        const auto read_status = read_video_capture_frame(s, cap, m, st);
        if (read_status == video_capture_read_status::wait_timeout) {
            continue;
        }
        if (read_status == video_capture_read_status::finished) {
            if (!st.stop_requested()
                && (s.get_type() != stream_type::file || s.is_looping())) {
                throw std::runtime_error(
                    "capture source ended unexpectedly (stream type: "
                    + stream::type_name(s.get_type()) + ")"
                );
            }
            break;
        }

        auto f = bgr_mat_to_frame(m);
        on_frame(std::move(f));
    }
}

std::shared_ptr<const grid_stream_index> opencv_client::get_grid_index_cached(
    const stream& s, const std::vector<line_ptr>& lines
) {
    using namespace opencv_client_support;

    grid_dims desired_dims = recommend_grid_dims(lines);
    std::uint64_t observed_generation = 0;

    {
        std::scoped_lock lock(grid_cache_mtx);

        auto it = grid_cache_by_stream.find(s.get_name());
        if (it != grid_cache_by_stream.end()) {
            const bool same_line_set
                = same_line_ptrs(it->second.line_snapshots, lines);
            const bool periodic_recalc_due = same_line_set
                && it->second.reuse_count >= grid_layout_recalc_interval;

            if (!same_line_set) {
                const line* focus_line = find_focus_line_for_rebuild(
                    it->second.line_snapshots, lines
                );
                desired_dims = recommend_grid_dims(lines, focus_line);
            }

            if (same_line_set && !periodic_recalc_due
                && it->second.dims.nx == desired_dims.nx
                && it->second.dims.ny == desired_dims.ny && it->second.index) {
                ++it->second.reuse_count;
                return it->second.index;
            }

            observed_generation = it->second.generation;
        }
    }

    auto rebuilt = std::make_shared<const grid_stream_index>(
        build_grid_stream_index(lines, desired_dims)
    );

    {
        std::scoped_lock lock(grid_cache_mtx);

        const auto current = grid_cache_by_stream.find(s.get_name());
        if (current != grid_cache_by_stream.end()
            && current->second.generation != observed_generation) {
            if (same_line_ptrs(current->second.line_snapshots, lines)
                && current->second.dims.nx == desired_dims.nx
                && current->second.dims.ny == desired_dims.ny
                && current->second.index) {
                ++current->second.reuse_count;
                return current->second.index;
            }

            // Another thread installed a newer snapshot while this one was
            // being built. The local immutable result remains safe for this
            // reader, but must not replace newer cache state.
            return rebuilt;
        }

        grid_cache_entry& e = grid_cache_by_stream[s.get_name()];
        e.dims = desired_dims;
        e.line_snapshots = lines;
        e.index = rebuilt;
        e.reuse_count = 0;
        e.generation = observed_generation + 1;

#ifdef YODAU_DEBUG_GRID
        std::cerr << "grid_index_rebuild stream=" << s.get_name()
                  << " lines=" << lines.size() << " dims=" << e.index->dims.nx
                  << "x" << e.index->dims.ny
                  << " segments=" << e.index->segments.size() << std::endl;
#endif

        return e.index;
    }
}

std::vector<event>
opencv_client::motion_processor(const stream& s, const frame& f) {
    std::vector<event> out;

    if (f.data.empty() || f.width <= 0 || f.height <= 0) {
        return out;
    }

    cv::Mat bgr;
    cv::Mat gray;
    if (!frame_to_bgr_gray_mats(f, bgr, gray)) {
        return out;
    }

    cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0.0);

#ifdef YODAU_DUMP_DEBUG_FRAMES
    static bool dumped_once = false;
    static std::optional<std::chrono::steady_clock::time_point> first_ts;

    bool do_dump = false;
    std::string base_name;

    {
        static std::mutex debug_dump_mtx;
        std::scoped_lock lock(debug_dump_mtx);

        if (!first_ts.has_value()) {
            first_ts = f.ts;
        }

        if (!dumped_once) {
            using namespace std::chrono;
            const auto elapsed = duration_cast<seconds>(f.ts - *first_ts);

            if (elapsed.count() >= 60) {
                do_dump = true;
                dumped_once = true;

                base_name = "debug_" + s.get_name() + "_t"
                    + std::to_string(elapsed.count());

                cv::imwrite(base_name + "_step0_bgr.png", bgr);

                cv::imwrite(base_name + "_step1_gray.png", gray);
            }
        }
    }
#endif

    cv::Mat prev_gray;
    if (!motion_state_.update_previous_gray(s.get_name(), gray, prev_gray)) {
        return out;
    }

    const cv::Mat diff
        = legacy_frame_delta_motion_mask(prev_gray, gray, 25, 1, 2);

#ifdef YODAU_DUMP_DEBUG_FRAMES
    if (do_dump && !base_name.empty()) {
        cv::imwrite(base_name + "_step2_mask.png", diff);
    }
#endif

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(
        diff, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE
    );

    if (contours.empty()) {
        return out;
    }

    const auto max_i_opt = largest_contour_index(contours);
    if (!max_i_opt.has_value()) {
        return out;
    }

    const size_t max_i = *max_i_opt;
    const double max_area = cv::contourArea(contours[max_i]);

    const double min_area = 0.001 * static_cast<double>(diff.rows * diff.cols);
    if (max_area < min_area) {
        return out;
    }

    std::vector<cv::Point> approx;
    {
        const double eps = 3.0;
        cv::approxPolyDP(contours[max_i], approx, eps, true);
    }

#ifdef YODAU_DUMP_DEBUG_FRAMES
    if (do_dump && !base_name.empty()) {
        cv::Mat contour_vis = bgr.clone();

        std::vector<std::vector<cv::Point>> approx_contours(1);
        approx_contours[0] = approx;

        cv::drawContours(
            contour_vis, approx_contours, 0, cv::Scalar(0, 255, 0), 2
        );

        cv::imwrite(base_name + "_step3_contours.png", contour_vis);
    }
#endif

    std::vector<point> contour_pct;
    contour_pct.reserve(approx.size());

    for (const auto& pt : approx) {
        const px_point pp { pt.x, pt.y };
        const point p = px_point_to_pct(pp, f.width, f.height);
        contour_pct.push_back(p);
    }

    bool motion_box_ok = false;
    const pct_bbox motion_box = compute_pct_bbox(contour_pct, motion_box_ok);

    const double ratio = motion_mask_ratio(diff);

    if (ratio < 0.01) {
        return out;
    }

    const double min_ratio = 0.02;
    if (ratio < min_ratio) {
        return out;
    }

    const auto now = std::chrono::steady_clock::now();
    if (!motion_state_.allow_motion_emit(
            s.get_name(), now, std::chrono::milliseconds(150)
        )) {
        return out;
    }

    cv::Moments mm = cv::moments(contours[max_i]);
    double cx = 0.0;
    double cy = 0.0;
    if (mm.m00 > 0.0) {
        cx = mm.m10 / mm.m00;
        cy = mm.m01 / mm.m00;
    } else {
        cx = static_cast<double>(f.width) * 0.5;
        cy = static_cast<double>(f.height) * 0.5;
    }

    const px_point cur_pos_px { static_cast<int>(std::lround(cx)),
                                static_cast<int>(std::lround(cy)) };
    const point cur_pos_pct = px_point_to_pct(cur_pos_px, f.width, f.height);

    point prev_pos {};
    const bool has_prev = motion_state_.update_motion_position(
        s.get_name(), cur_pos_pct, prev_pos
    );

    double impact_speed = 1.0;
    if (has_prev) {
        impact_speed
            = legacy_impact_speed(prev_pos, cur_pos_pct, ratio, min_ratio);
    }

    auto lines = s.lines_snapshot();
    normalize_lines_snapshot(lines);

    std::shared_ptr<const grid_stream_index> idx_snapshot;
    const grid_stream_index* idx_ptr = nullptr;
    grid_dims g = recommend_grid_dims(lines);
    if (has_prev && !lines.empty()) {
        idx_snapshot = get_grid_index_cached(s, lines);
        idx_ptr = idx_snapshot.get();
        g = idx_snapshot->dims;
    }

    const cv::Mat grid_u8 = downsample_motion_mask_to_grid(diff, g);
    const std::vector<int> active_cell_indices
        = active_motion_grid_cells(grid_u8, g);

    if (has_prev) {
#ifdef YODAU_GRID_PREFILTER
        std::vector<std::vector<size_t>> candidate_segments_by_line;
        grid_candidate_tracker tracker;
        std::vector<size_t> candidate_segment_ids;
        bool candidate_filter_ready = false;

        if (idx_ptr != nullptr) {
            const std::vector<int> candidate_cell_indices
                = tripwire_candidate_grid_cells(
                    active_cell_indices, contour_pct, prev_pos, cur_pos_pct, g
                );

            candidate_filter_ready = !candidate_cell_indices.empty();

            collect_grid_candidates(
                *idx_ptr, candidate_cell_indices, tracker, candidate_segment_ids
            );

            if (!candidate_segment_ids.empty()) {
                candidate_segments_by_line.assign(idx_ptr->lines.size(), {});

                for (const size_t seg_id : candidate_segment_ids) {
                    if (seg_id >= idx_ptr->segments.size()) {
                        continue;
                    }

                    const auto& ref = idx_ptr->segments[seg_id];
                    if (ref.line_index >= candidate_segments_by_line.size()) {
                        continue;
                    }

                    candidate_segments_by_line[ref.line_index].push_back(
                        ref.seg_index
                    );
                }
            }
        }
#endif

        for (size_t grid_li = 0; grid_li < lines.size(); ++grid_li) {
            const auto& lp = lines[grid_li];
            if (!lp) {
                continue;
            }

#ifdef YODAU_GRID_PREFILTER
            const grid_line_index* line_index = nullptr;
            const std::vector<size_t>* candidate_segments = nullptr;

            if (idx_ptr != nullptr) {
                if (grid_li >= idx_ptr->lines.size()) {
                    continue;
                }

                line_index = &idx_ptr->lines[grid_li].index;

                if (candidate_filter_ready) {
                    if (candidate_segments_by_line.empty()
                        || candidate_segments_by_line[grid_li].empty()) {
                        continue;
                    }
                    candidate_segments = &candidate_segments_by_line[grid_li];
                }
            }
#else
            const grid_line_index* line_index = nullptr;
            const std::vector<size_t>* candidate_segments = nullptr;
#endif

            const auto& pts = lp->points;
            if (pts.empty()) {
                continue;
            }

            if (motion_box_ok) {
#ifdef YODAU_GRID_PREFILTER
                bool do_fallback = true;

                if (idx_ptr) {
                    if (grid_li < idx_ptr->lines.size()) {
                        const auto& cl = idx_ptr->lines[grid_li];
                        if (cl.bbox_ok) {
                            const bool x_overlap
                                = !(cl.bbox.max_x < motion_box.min_x
                                    || cl.bbox.min_x > motion_box.max_x);

                            const bool y_overlap
                                = !(cl.bbox.max_y < motion_box.min_y
                                    || cl.bbox.min_y > motion_box.max_y);

                            if (!(x_overlap && y_overlap)) {
                                continue;
                            }

                            do_fallback = false;
                        }
                    }
                }

                if (do_fallback) {
                    bool line_box_ok = false;
                    const pct_bbox line_box
                        = compute_pct_bbox(pts, line_box_ok);
                    if (!line_box_ok) {
                        continue;
                    }

                    const bool x_overlap
                        = !(line_box.max_x < motion_box.min_x
                            || line_box.min_x > motion_box.max_x);

                    const bool y_overlap
                        = !(line_box.max_y < motion_box.min_y
                            || line_box.min_y > motion_box.max_y);

                    if (!(x_overlap && y_overlap)) {
                        continue;
                    }
                }
#else
                bool line_box_ok = false;
                const pct_bbox line_box = compute_pct_bbox(pts, line_box_ok);
                if (!line_box_ok) {
                    continue;
                }

                const bool x_overlap
                    = !(line_box.max_x < motion_box.min_x
                        || line_box.min_x > motion_box.max_x);

                const bool y_overlap
                    = !(line_box.max_y < motion_box.min_y
                        || line_box.min_y > motion_box.max_y);

                if (!(x_overlap && y_overlap)) {
                    continue;
                }
#endif
            }

            const auto crossings = tripwire_crossings_for_contour_line(
                *lp, prev_pos, cur_pos_pct, contour_pct, line_index,
                candidate_segments
            );
            if (crossings.empty()) {
                continue;
            }

            const std::string key
                = s.get_name() + "|" + tripwire_crossing_key(crossings.front());
            if (!motion_state_.allow_tripwire_emit(
                    key, now, std::chrono::milliseconds(1200)
                )) {
                continue;
            }

            for (const auto& crossing : crossings) {
                event tripwire_event = make_tripwire_event(
                    s.get_name(), crossing, now, impact_speed
                );

                // Tripwire events flow through the structured event sink.
                // Clients own filtering, retention, and presentation; avoid
                // an unconditional synchronous stderr side channel here.
                out.push_back(std::move(tripwire_event));
            }
        }
    }

    out.push_back(make_motion_event(s.get_name(), now, cur_pos_pct));

    const int max_bubbles = 40;

#ifdef YODAU_DEBUG_GRID
    if (!active_cell_indices.empty()) {
        if (!lines.empty()) {
            const auto idx = get_grid_index_cached(s, lines);

            grid_candidate_tracker tracker;
            std::vector<size_t> candidate_segment_ids;

            collect_grid_candidates(
                *idx, active_cell_indices, tracker, candidate_segment_ids
            );

            std::cerr << "grid_candidates stream=" << s.get_name()
                      << " active_cells=" << active_cell_indices.size()
                      << " segments=" << idx->segments.size()
                      << " candidates=" << candidate_segment_ids.size()
                      << std::endl;
        }
    }
#endif

    append_motion_grid_cell_events(
        out, s.get_name(), now, active_cell_indices, g, max_bubbles
    );

    return out;
}

stream_manager::daemon_start_fn opencv_client::daemon_start_fn() {
    return &opencv_client::daemon_start;
}

stream_manager::frame_processor_fn opencv_client::frame_processor_fn() {
    return std::bind_front(&opencv_client::motion_processor, this);
}

opencv_client& opencv_client::shared_instance() {
    static opencv_client instance;
    return instance;
}

void opencv_daemon_start(
    const stream& s, const std::function<void(frame&&)>& on_frame,
    const stop_token& st
) {
    opencv_client::daemon_start(s, on_frame, st);
}

std::vector<event> opencv_motion_processor(const stream& s, const frame& f) {
    return opencv_client::shared_instance().motion_processor(s, f);
}

}

#endif
