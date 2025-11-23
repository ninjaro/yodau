#ifdef YODAU_OPENCV

#include "opencv_client.hpp"

#include <charconv>
#include <filesystem>

#ifdef __linux__
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace yodau::backend {

#ifdef __linux__
namespace {
    [[maybe_unused]] bool is_capture_device(const std::string& path) {
        const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            return false;
        }

        v4l2_capability cap {};
        const int rc = ::ioctl(fd, VIDIOC_QUERYCAP, &cap);
        ::close(fd);

        if (rc < 0) {
            return false;
        }

        std::uint32_t caps = cap.capabilities;
        if (caps & V4L2_CAP_DEVICE_CAPS) {
            caps = cap.device_caps;
        }

        const bool capture = (caps & V4L2_CAP_VIDEO_CAPTURE)
            || (caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE);

        const bool streaming = (caps & V4L2_CAP_STREAMING);

        return capture && streaming;
    }
}
#endif

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
    return (a <= c + point::epsilon && c <= b + point::epsilon)
        || (b <= c + point::epsilon && c <= a + point::epsilon);
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
    const point& a, const point& b
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

        consider_hit(
            hit, best_dist2, best_a, best_b, best_pos, cur_pos_pct, a, b, ip
        );
    }
}

void opencv_client::process_tripwire_for_line(
    std::vector<event>& out, const stream& s, const line& l,
    const point& prev_pos, const point& cur_pos_pct,
    const std::vector<point>& contour_pct,
    const std::chrono::steady_clock::time_point now
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

    for (size_t i = 1; i < pts.size(); ++i) {
        test_line_segment_against_contour(
            hit, best_dist2, best_a, best_b, best_pos, cur_pos_pct, contour_pct,
            pts[i - 1], pts[i]
        );
    }

    if (l.closed && pts.size() > 2) {
        test_line_segment_against_contour(
            hit, best_dist2, best_a, best_b, best_pos, cur_pos_pct, contour_pct,
            pts.back(), pts.front()
        );
    }

    if (!hit) {
        return;
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

    event t;
    t.kind = event_kind::tripwire;
    t.stream_name = s.get_name();
    t.line_name = l.name;
    t.ts = now;
    t.pos_pct = best_pos;
    t.message = dir;

    std::cerr << "tripwire stream=" << t.stream_name << " line=" << t.line_name
              << " dir=" << dir << std::endl;

    out.push_back(std::move(t));
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

std::vector<event>
opencv_client::motion_processor(const stream& s, const frame& f) {
    std::vector<event> out;

    if (f.data.empty() || f.width <= 0 || f.height <= 0) {
        return out;
    }

    cv::Mat bgr(
        f.height, f.width, CV_8UC3, const_cast<std::uint8_t*>(f.data.data()),
        static_cast<size_t>(f.stride)
    );

    cv::Mat gray;
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0.0);

    cv::Mat prev_gray;
    {
        std::scoped_lock lock(mtx);
        auto it = prev_gray_by_stream.find(s.get_name());
        if (it == prev_gray_by_stream.end()) {
            prev_gray_by_stream.emplace(s.get_name(), gray.clone());
            return out;
        }
        prev_gray = it->second;
        it->second = gray.clone();
    }

    cv::Mat diff;
    cv::absdiff(prev_gray, gray, diff);
    cv::threshold(diff, diff, 25, 255, cv::THRESH_BINARY);

    cv::erode(diff, diff, cv::Mat(), cv::Point(-1, -1), 1);
    cv::dilate(diff, diff, cv::Mat(), cv::Point(-1, -1), 2);

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

    std::vector<point> contour_pct;
    contour_pct.reserve(approx.size());

    for (const auto& pt : approx) {
        point p;
        p.x = static_cast<float>(pt.x) * 100.0f / static_cast<float>(f.width);
        p.y = static_cast<float>(pt.y) * 100.0f / static_cast<float>(f.height);
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

    const point cur_pos_pct { static_cast<float>(cx * 100.0 / f.width),
                              static_cast<float>(cy * 100.0 / f.height) };

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

    if (has_prev) {
        const auto lines = s.lines_snapshot();
        for (const auto& lp : lines) {
            if (!lp) {
                continue;
            }

            const auto& pts = lp->points;
            if (pts.empty()) {
                continue;
            }

            if (motion_box_ok) {
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

            process_tripwire_for_line(
                out, s, *lp, prev_pos, cur_pos_pct, contour_pct, now
            );
        }
    }

    add_motion_event(out, s.get_name(), now, cur_pos_pct);

    const int grid_step = 24;
    const int max_bubbles = 80;

    int bubbled = 0;
    for (int y = 0; y < diff.rows; y += grid_step) {
        const std::uint8_t* row = diff.ptr<std::uint8_t>(y);
        for (int x = 0; x < diff.cols; x += grid_step) {
            if (row[x] == 0) {
                continue;
            }

            point p;
            p.x = static_cast<float>(x) * 100.0f / static_cast<float>(f.width);
            p.y = static_cast<float>(y) * 100.0f / static_cast<float>(f.height);

            add_motion_event(out, s.get_name(), now, p);
            bubbled++;

            if (bubbled >= max_bubbles) {
                break;
            }
        }

        if (bubbled >= max_bubbles) {
            break;
        }
    }

    return out;
}

stream_manager::daemon_start_fn opencv_client::daemon_start_fn() {
    return [this](
               const stream& s, std::function<void(frame&&)> on_frame,
               std::stop_token st
           ) { daemon_start(s, on_frame, st); };
}

stream_manager::frame_processor_fn opencv_client::frame_processor_fn() {
    return [this](const stream& s, const frame& f) {
        return motion_processor(s, f);
    };
}

namespace {
    opencv_client& global_opencv_client() {
        static opencv_client inst;
        return inst;
    }
}

void opencv_daemon_start(
    const stream& s, const std::function<void(frame&&)>& on_frame,
    const std::stop_token& st
) {
    global_opencv_client().daemon_start(s, on_frame, st);
}

std::vector<event> opencv_motion_processor(const stream& s, const frame& f) {
    return global_opencv_client().motion_processor(s, f);
}

}

#endif
