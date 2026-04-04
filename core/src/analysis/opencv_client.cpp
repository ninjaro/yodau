#ifdef YODAU_OPENCV

#include "analysis/opencv_client.hpp"
#include "analysis/tripwire_grid_stream_index.hpp"
#include "geometry/coords.hpp"
#include "streams/linux_capture_device.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>

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
    const std::vector<const line*>& lhs, const std::vector<const line*>& rhs
) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (size_t i = 0; i < lhs.size(); ++i) {
        if (lhs[i] != rhs[i]) {
            return false;
        }
    }

    return true;
}

const line* find_focus_line_for_rebuild(
    const std::vector<const line*>& previous_line_ptrs,
    const std::vector<line_ptr>& current_lines
) {
    const line* focus_line = nullptr;

    for (const auto& line_ptr_value : current_lines) {
        if (!line_ptr_value) {
            continue;
        }

        bool seen_before = false;
        for (const line* previous_ptr : previous_line_ptrs) {
            if (previous_ptr == line_ptr_value.get()) {
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

void add_grid_cell_index(
    std::vector<std::uint8_t>& seen_cells, std::vector<int>& out_cell_indices,
    const grid_dims& dims, const grid_point& cell
) {
    if (dims.nx <= 0 || dims.ny <= 0) {
        return;
    }

    const int clamped_x = clamp_int(cell.x, 0, dims.nx - 1);
    const int clamped_y = clamp_int(cell.y, 0, dims.ny - 1);
    const int cell_idx = clamped_y * dims.nx + clamped_x;
    if (cell_idx < 0) {
        return;
    }

    const size_t index = static_cast<size_t>(cell_idx);
    if (index >= seen_cells.size() || seen_cells[index] != 0) {
        return;
    }

    seen_cells[index] = 1;
    out_cell_indices.push_back(cell_idx);
}

void add_grid_trace_cells(
    std::vector<std::uint8_t>& seen_cells, std::vector<int>& out_cell_indices,
    const grid_dims& dims, const point& a_pct, const point& b_pct
) {
    const auto cells = trace_grid_cells_pct(a_pct, b_pct, dims);
    for (const auto& cell : cells) {
        add_grid_cell_index(seen_cells, out_cell_indices, dims, cell);
    }
}

void add_polyline_cells(
    std::vector<std::uint8_t>& seen_cells, std::vector<int>& out_cell_indices,
    const grid_dims& dims, const std::vector<point>& pts, const bool closed
) {
    if (pts.empty()) {
        return;
    }

    if (pts.size() == 1) {
        add_grid_cell_index(
            seen_cells, out_cell_indices, dims, pct_point_to_grid(pts.front(), dims)
        );
        return;
    }

    for (size_t i = 1; i < pts.size(); ++i) {
        add_grid_trace_cells(
            seen_cells, out_cell_indices, dims, pts[i - 1], pts[i]
        );
    }

    if (closed && pts.size() > 2) {
        add_grid_trace_cells(
            seen_cells, out_cell_indices, dims, pts.back(), pts.front()
        );
    }
}

void add_motion_path_cells(
    std::vector<std::uint8_t>& seen_cells, std::vector<int>& out_cell_indices,
    const grid_dims& dims, const point& previous_pos, const point& current_pos
) {
    add_grid_trace_cells(
        seen_cells, out_cell_indices, dims, previous_pos, current_pos
    );
}

bool frame_to_bgr_and_gray(
    const frame& frame_value, cv::Mat& bgr, cv::Mat& gray
) {
    if (frame_value.width <= 0 || frame_value.height <= 0
        || frame_value.stride <= 0 || frame_value.data.empty()) {
        return false;
    }

    auto* bytes = const_cast<std::uint8_t*>(frame_value.data.data());

    switch (frame_value.format) {
    case pixel_format::gray8: {
        cv::Mat input(
            frame_value.height, frame_value.width, CV_8UC1, bytes,
            static_cast<size_t>(frame_value.stride)
        );
        gray = input.clone();
        cv::cvtColor(input, bgr, cv::COLOR_GRAY2BGR);
        return true;
    }
    case pixel_format::rgb24: {
        cv::Mat input(
            frame_value.height, frame_value.width, CV_8UC3, bytes,
            static_cast<size_t>(frame_value.stride)
        );
        cv::cvtColor(input, bgr, cv::COLOR_RGB2BGR);
        cv::cvtColor(input, gray, cv::COLOR_RGB2GRAY);
        return true;
    }
    case pixel_format::bgr24: {
        cv::Mat input(
            frame_value.height, frame_value.width, CV_8UC3, bytes,
            static_cast<size_t>(frame_value.stride)
        );
        bgr = input;
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
        return true;
    }
    case pixel_format::rgba32: {
        cv::Mat input(
            frame_value.height, frame_value.width, CV_8UC4, bytes,
            static_cast<size_t>(frame_value.stride)
        );
        cv::cvtColor(input, bgr, cv::COLOR_RGBA2BGR);
        cv::cvtColor(input, gray, cv::COLOR_RGBA2GRAY);
        return true;
    }
    case pixel_format::bgra32: {
        cv::Mat input(
            frame_value.height, frame_value.width, CV_8UC4, bytes,
            static_cast<size_t>(frame_value.stride)
        );
        cv::cvtColor(input, bgr, cv::COLOR_BGRA2BGR);
        cv::cvtColor(input, gray, cv::COLOR_BGRA2GRAY);
        return true;
    }
    }

    return false;
}

} // namespace opencv_client_support

cv::Mat
opencv_client::downsample_to_grid_u8(const cv::Mat& diff, const grid_dims& g) {
    cv::Mat out;
    if (g.nx <= 0 || g.ny <= 0 || diff.empty()) {
        return out;
    }

    cv::resize(diff, out, cv::Size(g.nx, g.ny), 0.0, 0.0, cv::INTER_AREA);
    return out;
}

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

int opencv_client::local_index_from_path(const std::string& path) const {
    const std::string pref = "/dev/video";
    if (path.rfind(pref, 0) != 0) {
        return -1;
    }

    const auto tail = path.substr(pref.size());
    int idx = -1;

    const auto res
        = std::from_chars(tail.data(), tail.data() + tail.size(), idx);
    if (res.ec != std::errc() || res.ptr != tail.data() + tail.size()) {
        return -1;
    }

    return idx;
}

frame opencv_client::mat_to_frame(const cv::Mat& m) const {
    frame f;
    f.width = m.cols;
    f.height = m.rows;
    f.stride = static_cast<int>(m.step);
    f.ts = std::chrono::steady_clock::now();

    if (m.channels() == 3 && m.type() == CV_8UC3) {
        f.format = pixel_format::bgr24;
        f.data.assign(m.data, m.data + m.total() * m.elemSize());
        return f;
    }

    cv::Mat bgr;
    if (m.channels() == 1) {
        cv::cvtColor(m, bgr, cv::COLOR_GRAY2BGR);
    } else if (m.channels() == 4) {
        cv::cvtColor(m, bgr, cv::COLOR_BGRA2BGR);
    } else {
        m.convertTo(bgr, CV_8UC3);
    }

    f.format = pixel_format::bgr24;
    f.stride = static_cast<int>(bgr.step);
    f.data.assign(bgr.data, bgr.data + bgr.total() * bgr.elemSize());
    return f;
}

void opencv_client::daemon_start(
    const stream& s, const std::function<void(frame&&)>& on_frame,
    const std::stop_token& st
) {
    const auto path = s.get_path();
    cv::VideoCapture cap;

    const auto idx = local_index_from_path(path);
    if (idx >= 0) {
#ifdef __linux__
        if (!is_linux_capture_device(path)) {
            return;
        }
#endif
        cap.open(idx);
    } else {
        cap.open(path);
    }

    if (!cap.isOpened()) {
        return;
    }

    cv::Mat m;
    while (!st.stop_requested()) {
        if (!cap.read(m) || m.empty()) {
            if (s.is_looping() && s.get_type() == stream_type::file) {
                cap.set(cv::CAP_PROP_POS_FRAMES, 0);
                continue;
            }
            break;
        }

        auto f = mat_to_frame(m);
        on_frame(std::move(f));
    }
}

float opencv_client::cross_z(
    const point& a, const point& b, const point& c
) const {
    const float abx = b.x - a.x;
    const float aby = b.y - a.y;
    const float acx = c.x - a.x;
    const float acy = c.y - a.y;
    return abx * acy - aby * acx;
}

int opencv_client::orient(
    const point& a, const point& b, const point& c
) const {
    const float v = cross_z(a, b, c);
    if (v > point::epsilon) {
        return 1;
    }
    if (v < -point::epsilon) {
        return -1;
    }
    return 0;
}

bool opencv_client::between(float a, float b, float c) const {
    auto [lo, hi] = std::minmax(a, b);
    return lo <= c + point::epsilon && c <= hi + point::epsilon;
    // return (a <= c + point::epsilon && c <= b + point::epsilon)
    //     || (b <= c + point::epsilon && c <= a + point::epsilon);
}

bool opencv_client::on_segment(
    const point& a, const point& b, const point& c
) const {
    return orient(a, b, c) == 0 && between(a.x, b.x, c.x)
        && between(a.y, b.y, c.y);
}

bool opencv_client::segments_intersect(
    const point& p1, const point& p2, const point& q1, const point& q2
) const {
    const int o1 = orient(p1, p2, q1);
    const int o2 = orient(p1, p2, q2);
    const int o3 = orient(q1, q2, p1);
    const int o4 = orient(q1, q2, p2);

    if (o1 != o2 && o3 != o4) {
        return true;
    }

    if (o1 == 0 && on_segment(p1, p2, q1)) {
        return true;
    }
    if (o2 == 0 && on_segment(p1, p2, q2)) {
        return true;
    }
    if (o3 == 0 && on_segment(q1, q2, p1)) {
        return true;
    }
    if (o4 == 0 && on_segment(q1, q2, p2)) {
        return true;
    }

    return false;
}

std::optional<point> opencv_client::segment_intersection(
    const point& p1, const point& p2, const point& q1, const point& q2
) const {
    const float rpx = p2.x - p1.x;
    const float rpy = p2.y - p1.y;
    const float spx = q2.x - q1.x;
    const float spy = q2.y - q1.y;

    const float den = rpx * spy - rpy * spx;
    if (std::abs(den) <= point::epsilon) {
        return {};
    }

    const float qpx = q1.x - p1.x;
    const float qpy = q1.y - p1.y;

    const float t = (qpx * spy - qpy * spx) / den;
    const float u = (qpx * rpy - qpy * rpx) / den;

    if (t < -point::epsilon || t > 1.0f + point::epsilon) {
        return {};
    }
    if (u < -point::epsilon || u > 1.0f + point::epsilon) {
        return {};
    }

    point out;
    out.x = p1.x + t * rpx;
    out.y = p1.y + t * rpy;
    return out;
}

void opencv_client::add_motion_event(
    std::vector<event>& out, const std::string& stream_name,
    const std::chrono::steady_clock::time_point ts, const point& pos_pct
) const {
    event e;
    e.kind = event_kind::motion;
    e.stream_name = stream_name;
    e.ts = ts;
    e.pos_pct = pos_pct;
    out.push_back(std::move(e));
}

void opencv_client::consider_hit(
    bool& hit, float& best_dist2, point& best_a, point& best_b, point& best_pos,
    const point& cur_pos_pct, const point& a, const point& b, const point& pos
) const {
    const float dx = pos.x - cur_pos_pct.x;
    const float dy = pos.y - cur_pos_pct.y;
    const float d2 = dx * dx + dy * dy;

    if (d2 < best_dist2) {
        best_dist2 = d2;
        best_a = a;
        best_b = b;
        best_pos = pos;
        hit = true;
    }
}

void opencv_client::test_line_segment_against_contour(
    bool& hit, float& best_dist2, point& best_a, point& best_b, point& best_pos,
    const point& cur_pos_pct, const std::vector<point>& contour_pct,
    const point& a, const point& b, std::vector<point>& hit_positions_pct
) const {
    if (contour_pct.size() < 2) {
        return;
    }

    for (size_t j = 1; j < contour_pct.size(); ++j) {
        const auto& c1 = contour_pct[j - 1];
        const auto& c2 = contour_pct[j];

        if (segments_intersect(a, b, c1, c2)) {
            point ip = cur_pos_pct;
            const auto inter = segment_intersection(a, b, c1, c2);
            if (inter.has_value()) {
                ip = *inter;
            }

            hit_positions_pct.push_back(ip);

            consider_hit(
                hit, best_dist2, best_a, best_b, best_pos, cur_pos_pct, a, b, ip
            );
        }
    }

    const auto& c_last = contour_pct.back();
    const auto& c_first = contour_pct.front();
    if (segments_intersect(a, b, c_last, c_first)) {
        point ip = cur_pos_pct;
        const auto inter = segment_intersection(a, b, c_last, c_first);
        if (inter.has_value()) {
            ip = *inter;
        }

        hit_positions_pct.push_back(ip);

        consider_hit(
            hit, best_dist2, best_a, best_b, best_pos, cur_pos_pct, a, b, ip
        );
    }
}

void opencv_client::process_tripwire_for_line(
    std::vector<event>& out, const stream& s, const line& l,
    const point& prev_pos, const point& cur_pos_pct,
    const std::vector<point>& contour_pct,
    const std::chrono::steady_clock::time_point now, double impact_speed,
    const grid_line_index* line_index,
    const std::vector<size_t>* candidate_segment_indices
) {
    const auto& pts = l.points;
    if (pts.size() < 2) {
        return;
    }

    bool hit = false;
    point best_a {};
    point best_b {};
    point best_pos = cur_pos_pct;
    float best_dist2 = std::numeric_limits<float>::max();

    std::vector<point> hit_positions_pct;

    if (line_index != nullptr && candidate_segment_indices != nullptr
        && !candidate_segment_indices->empty()) {
        for (const size_t segment_index : *candidate_segment_indices) {
            if (segment_index >= line_index->segments.size()) {
                continue;
            }

            const auto& segment = line_index->segments[segment_index];
            test_line_segment_against_contour(
                hit, best_dist2, best_a, best_b, best_pos, cur_pos_pct,
                contour_pct, segment.a_pct, segment.b_pct, hit_positions_pct
            );
        }
    } else {
        for (size_t i = 1; i < pts.size(); ++i) {
            test_line_segment_against_contour(
                hit, best_dist2, best_a, best_b, best_pos, cur_pos_pct,
                contour_pct, pts[i - 1], pts[i], hit_positions_pct
            );
        }

        if (l.closed && pts.size() > 2) {
            test_line_segment_against_contour(
                hit, best_dist2, best_a, best_b, best_pos, cur_pos_pct,
                contour_pct, pts.back(), pts.front(), hit_positions_pct
            );
        }
    }

    if (!hit) {
        return;
    }

    if (hit_positions_pct.empty()) {
        hit_positions_pct.push_back(best_pos);
    }

    double geom_strength = 1.0;

    if (!hit_positions_pct.empty()) {
        float min_x = hit_positions_pct[0].x;
        float max_x = hit_positions_pct[0].x;
        float min_y = hit_positions_pct[0].y;
        float max_y = hit_positions_pct[0].y;

        for (size_t i = 1; i < hit_positions_pct.size(); ++i) {
            const auto& hp = hit_positions_pct[i];
            if (hp.x < min_x) {
                min_x = hp.x;
            }
            if (hp.x > max_x) {
                max_x = hp.x;
            }
            if (hp.y < min_y) {
                min_y = hp.y;
            }
            if (hp.y > max_y) {
                max_y = hp.y;
            }
        }

        double dx = static_cast<double>(max_x - min_x);
        double dy = static_cast<double>(max_y - min_y);
        double span = std::sqrt(dx * dx + dy * dy);

        const double span_min = 1.0;
        const double span_max = 20.0;

        if (span < span_min) {
            span = span_min;
        }

        double norm = span / span_max;
        if (norm < 0.0) {
            norm = 0.0;
        } else if (norm > 1.0) {
            norm = 1.0;
        }

        double mapped = 0.5 + norm * 0.5;
        if (mapped < 0.5) {
            mapped = 0.5;
        } else if (mapped > 1.0) {
            mapped = 1.0;
        }

        geom_strength = mapped;
    }

    const float prev_side = cross_z(best_a, best_b, prev_pos);
    const float cur_side = cross_z(best_a, best_b, cur_pos_pct);

    std::string dir = "flat";
    if (prev_side <= 0.0f && cur_side > 0.0f) {
        dir = "neg_to_pos";
    } else if (prev_side >= 0.0f && cur_side < 0.0f) {
        dir = "pos_to_neg";
    }

    if (l.dir == tripwire_dir::neg_to_pos) {
        if (dir != "neg_to_pos") {
            return;
        }
    } else if (l.dir == tripwire_dir::pos_to_neg) {
        if (dir != "pos_to_neg") {
            return;
        }
    }

    const int tripwire_cooldown_ms = 1200;
    const std::string key = s.get_name() + "|" + l.name + "|" + dir;

    bool allow_tripwire = true;
    {
        std::scoped_lock lock(mtx);
        auto it = last_tripwire_by_key.find(key);
        if (it != last_tripwire_by_key.end()) {
            const auto dt
                = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now - it->second
                )
                      .count();
            if (dt < tripwire_cooldown_ms) {
                allow_tripwire = false;
            }
        }

        if (allow_tripwire) {
            last_tripwire_by_key[key] = now;
        }
    }

    if (!allow_tripwire) {
        return;
    }

    double strength_to_use = geom_strength;

    for (const auto& pos : hit_positions_pct) {
        event t;
        t.kind = event_kind::tripwire;
        t.stream_name = s.get_name();
        t.line_name = l.name;
        t.ts = now;
        t.pos_pct = pos;

        std::string msg = dir;
        msg.push_back('|');
        msg += std::to_string(strength_to_use);
        msg.push_back('|');
        msg += std::to_string(impact_speed);
        t.message = msg;

        std::cerr << "tripwire stream=" << t.stream_name
                  << " line=" << t.line_name << " dir=" << dir << std::endl;

        out.push_back(std::move(t));
    }
}

std::optional<size_t> opencv_client::find_largest_contour_index(
    const std::vector<std::vector<cv::Point>>& contours
) const {
    if (contours.empty()) {
        return {};
    }

    double max_area = 0.0;
    size_t max_i = 0;

    for (size_t i = 0; i < contours.size(); ++i) {
        const double area = cv::contourArea(contours[i]);
        if (area > max_area) {
            max_area = area;
            max_i = i;
        }
    }

    return max_i;
}

const grid_stream_index&
opencv_client::get_grid_index_cached(
    const stream& s, const std::vector<line_ptr>& lines
) {
    using namespace opencv_client_support;

    std::vector<const line*> ptrs;
    ptrs.reserve(lines.size());

    for (const auto& lp : lines) {
        if (!lp) {
            continue;
        }
        ptrs.push_back(lp.get());
    }

    grid_dims desired_dims = recommend_grid_dims(lines);

    {
        std::scoped_lock lock(grid_cache_mtx);

        auto it = grid_cache_by_stream.find(s.get_name());
        if (it != grid_cache_by_stream.end()) {
            const bool same_line_set
                = same_line_ptrs(it->second.line_ptrs, ptrs);
            const bool periodic_recalc_due
                = same_line_set
                && it->second.reuse_count >= grid_layout_recalc_interval;

            if (!same_line_set) {
                const line* focus_line
                    = find_focus_line_for_rebuild(it->second.line_ptrs, lines);
                desired_dims = recommend_grid_dims(lines, focus_line);
            }

            if (same_line_set && !periodic_recalc_due
                && it->second.dims.nx == desired_dims.nx
                && it->second.dims.ny == desired_dims.ny) {
                ++it->second.reuse_count;
                return it->second.index;
            }
        }
    }

    const grid_stream_index rebuilt = build_grid_stream_index(lines, desired_dims);

    {
        std::scoped_lock lock(grid_cache_mtx);

        grid_cache_entry& e = grid_cache_by_stream[s.get_name()];
        e.dims = desired_dims;
        e.line_ptrs = std::move(ptrs);
        e.index = rebuilt;
        e.reuse_count = 0;

#ifdef YODAU_DEBUG_GRID
        std::cerr << "grid_index_rebuild stream=" << s.get_name()
                  << " lines=" << lines.size()
                  << " dims=" << e.index.dims.nx << "x" << e.index.dims.ny
                  << " segments=" << e.index.segments.size() << std::endl;
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
    if (!opencv_client_support::frame_to_bgr_and_gray(f, bgr, gray)) {
        return out;
    }

    cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0.0);

#ifdef YODAU_DUMP_DEBUG_FRAMES
    static bool dumped_once = false;
    static std::optional<std::chrono::steady_clock::time_point> first_ts;

    bool do_dump = false;
    std::string base_name;

    {
        std::scoped_lock lock(mtx);

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
    {
        std::scoped_lock lock(mtx);
        auto it = prev_gray_by_stream.find(s.get_name());
        if (it == prev_gray_by_stream.end()) {
            prev_gray_by_stream.emplace(s.get_name(), gray.clone());
            return out;
        }
        prev_gray = it->second;

        if (prev_gray.empty() || prev_gray.size() != gray.size()
            || prev_gray.type() != gray.type()) {
            it->second = gray.clone();
            return out;
        }

        it->second = gray.clone();
    }

    cv::Mat diff;
    cv::absdiff(prev_gray, gray, diff);
    cv::threshold(diff, diff, 25, 255, cv::THRESH_BINARY);

    cv::erode(diff, diff, cv::Mat(), cv::Point(-1, -1), 1);
    cv::dilate(diff, diff, cv::Mat(), cv::Point(-1, -1), 2);

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

    const auto max_i_opt = find_largest_contour_index(contours);
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

    struct bbox2f {
        float min_x;
        float min_y;
        float max_x;
        float max_y;
    };

    bbox2f motion_box {};
    bool motion_box_ok = false;

    if (!contour_pct.empty()) {
        motion_box.min_x = 100.0f;
        motion_box.min_y = 100.0f;
        motion_box.max_x = 0.0f;
        motion_box.max_y = 0.0f;

        for (const auto& p : contour_pct) {
            if (p.x < motion_box.min_x) {
                motion_box.min_x = p.x;
            }
            if (p.y < motion_box.min_y) {
                motion_box.min_y = p.y;
            }
            if (p.x > motion_box.max_x) {
                motion_box.max_x = p.x;
            }
            if (p.y > motion_box.max_y) {
                motion_box.max_y = p.y;
            }
        }

        motion_box_ok = true;
    }

    const int nz = cv::countNonZero(diff);
    const int total = diff.rows * diff.cols;
    const double ratio = total > 0 ? static_cast<double>(nz) / total : 0.0;

    if (ratio < 0.01) {
        return out;
    }

    const double min_ratio = 0.02;
    const int cooldown_ms = 150;

    if (ratio < min_ratio) {
        return out;
    }

    const auto now = std::chrono::steady_clock::now();
    {
        std::scoped_lock lock(mtx);
        auto it = last_emit_by_stream.find(s.get_name());
        if (it != last_emit_by_stream.end()) {
            const auto dt
                = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now - it->second
                )
                      .count();
            if (dt < cooldown_ms) {
                return out;
            }
        }
        last_emit_by_stream[s.get_name()] = now;
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
    bool has_prev = false;
    {
        std::scoped_lock lock(mtx);
        auto it = last_pos_by_stream.find(s.get_name());
        if (it != last_pos_by_stream.end()) {
            prev_pos = it->second;
            has_prev = true;
        }
        last_pos_by_stream[s.get_name()] = cur_pos_pct;
    }

    double impact_speed = 1.0;
    if (has_prev) {
        const double dx = static_cast<double>(cur_pos_pct.x - prev_pos.x);
        const double dy = static_cast<double>(cur_pos_pct.y - prev_pos.y);
        const double distance_pct = std::sqrt(dx * dx + dy * dy);
        const double ratio_boost = std::clamp((ratio - min_ratio) * 18.0, 0.0, 1.2);
        impact_speed = std::clamp(0.35 + distance_pct / 8.0 + ratio_boost, 0.35, 2.5);
    }

    auto lines = s.lines_snapshot();
    normalize_lines_snapshot(lines);

    const grid_stream_index* idx_ptr = nullptr;
    grid_dims g = recommend_grid_dims(lines);
    if (has_prev && !lines.empty()) {
        const grid_stream_index& idx_ref = get_grid_index_cached(s, lines);
        idx_ptr = &idx_ref;
        g = idx_ref.dims;
    }

    const cv::Mat grid_u8 = downsample_to_grid_u8(diff, g);

    std::vector<int> active_cell_indices;
    active_cell_indices.reserve(static_cast<size_t>(g.nx * g.ny));

    if (!grid_u8.empty()) {
        for (int cell_y = 0; cell_y < g.ny; ++cell_y) {
            const std::uint8_t* row = grid_u8.ptr<std::uint8_t>(cell_y);
            for (int cell_x = 0; cell_x < g.nx; ++cell_x) {
                if (row[cell_x] == 0) {
                    continue;
                }

                active_cell_indices.push_back(cell_y * g.nx + cell_x);
            }
        }
    }

    if (has_prev) {
#ifdef YODAU_GRID_PREFILTER
        std::vector<std::vector<size_t>> candidate_segments_by_line;
        grid_candidate_tracker tracker;
        std::vector<size_t> candidate_segment_ids;
        std::vector<int> candidate_cell_indices;
        bool candidate_filter_ready = false;

        if (idx_ptr != nullptr) {
            std::vector<std::uint8_t> seen_cells(
                static_cast<size_t>(g.nx * g.ny), 0
            );

            candidate_cell_indices.reserve(
                active_cell_indices.size() + contour_pct.size() + 8
            );

            for (const int cell_idx : active_cell_indices) {
                const int cell_x = cell_idx % g.nx;
                const int cell_y = cell_idx / g.nx;
                opencv_client_support::add_grid_cell_index(
                    seen_cells, candidate_cell_indices, g,
                    grid_point { cell_x, cell_y }
                );
            }

            opencv_client_support::add_polyline_cells(
                seen_cells, candidate_cell_indices, g, contour_pct, true
            );
            opencv_client_support::add_motion_path_cells(
                seen_cells, candidate_cell_indices, g, prev_pos, cur_pos_pct
            );

            candidate_filter_ready = !candidate_cell_indices.empty();

            collect_grid_candidates(
                *idx_ptr, candidate_cell_indices, tracker,
                candidate_segment_ids
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
                    bbox2f line_box {};
                    line_box.min_x = 100.0f;
                    line_box.min_y = 100.0f;
                    line_box.max_x = 0.0f;
                    line_box.max_y = 0.0f;

                    for (const auto& p : pts) {
                        if (p.x < line_box.min_x) {
                            line_box.min_x = p.x;
                        }
                        if (p.y < line_box.min_y) {
                            line_box.min_y = p.y;
                        }
                        if (p.x > line_box.max_x) {
                            line_box.max_x = p.x;
                        }
                        if (p.y > line_box.max_y) {
                            line_box.max_y = p.y;
                        }
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
                bbox2f line_box {};
                line_box.min_x = 100.0f;
                line_box.min_y = 100.0f;
                line_box.max_x = 0.0f;
                line_box.max_y = 0.0f;

                for (const auto& p : pts) {
                    if (p.x < line_box.min_x) {
                        line_box.min_x = p.x;
                    }
                    if (p.y < line_box.min_y) {
                        line_box.min_y = p.y;
                    }
                    if (p.x > line_box.max_x) {
                        line_box.max_x = p.x;
                    }
                    if (p.y > line_box.max_y) {
                        line_box.max_y = p.y;
                    }
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

            process_tripwire_for_line(
                out, s, *lp, prev_pos, cur_pos_pct, contour_pct, now,
                impact_speed,
                line_index, candidate_segments
            );
        }
    }

    add_motion_event(out, s.get_name(), now, cur_pos_pct);

    const int max_bubbles = 40;

#ifdef YODAU_DEBUG_GRID
    if (!active_cell_indices.empty()) {
        if (!lines.empty()) {
            const grid_stream_index& idx = get_grid_index_cached(s, lines);

            grid_candidate_tracker tracker;
            std::vector<size_t> candidate_segment_ids;

            collect_grid_candidates(
                idx, active_cell_indices, tracker, candidate_segment_ids
            );

            std::cerr << "grid_candidates stream=" << s.get_name()
                      << " active_cells=" << active_cell_indices.size()
                      << " segments=" << idx.segments.size()
                      << " candidates=" << candidate_segment_ids.size()
                      << std::endl;
        }
    }
#endif

    int bubbled = 0;
    for (const int cell_idx : active_cell_indices) {
        const int cell_x = cell_idx % g.nx;
        const int cell_y = cell_idx / g.nx;

        const grid_point c { cell_x, cell_y };
        const point p = grid_cell_center_pct(c, g);

        add_motion_event(out, s.get_name(), now, p);
        bubbled++;

        if (bubbled >= max_bubbles) {
            break;
        }
    }

    return out;
}

stream_manager::daemon_start_fn opencv_client::daemon_start_fn() {
    return std::bind_front(&opencv_client::daemon_start, this);
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
    const std::stop_token& st
) {
    opencv_client::shared_instance().daemon_start(s, on_frame, st);
}

std::vector<event> opencv_motion_processor(const stream& s, const frame& f) {
    return opencv_client::shared_instance().motion_processor(s, f);
}

}

#endif
