#ifndef YODAU_BACKEND_OPENCV_CLIENT_HPP
#define YODAU_BACKEND_OPENCV_CLIENT_HPP

#ifdef YODAU_OPENCV

#include "event.hpp"
#include "frame.hpp"
#include "stream.hpp"
#include "stream_manager.hpp"

#include <opencv2/opencv.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <unordered_map>
#include <vector>

namespace yodau::backend {

/**
 * @brief OpenCV-based backend helper for capture and motion/tripwire analytics.
 *
 * This class provides:
 * - a frame-producing daemon for a given @ref stream using @c cv::VideoCapture,
 * - a motion / tripwire frame processor that returns @ref event objects,
 * - adapters returning hooks compatible with @ref stream_manager.
 *
 * The implementation keeps per-stream state (previous gray frame, last emit
 * time, last motion position, and per-tripwire cooldowns) protected by an
 * internal mutex.
 *
 * Coordinate system:
 * - Motion and tripwire positions are reported as percentage-based points
 *   (@ref point), i.e. x,y in [0.0; 100.0].
 *
 * Thread-safety:
 * - Public methods are safe to call concurrently; internal maps are guarded by
 *   @ref mtx.
 */
class opencv_client {
public:
    /**
     * @brief Default constructor.
     *
     * No heavy initialization is performed. Internal state is created lazily.
     */
    opencv_client() = default;

    /**
     * @brief Start capturing frames from a stream and push them to a callback.
     *
     * The daemon:
     * - opens a @c cv::VideoCapture either by local index (for "/dev/videoN")
     *   or directly by path/URL,
     * - reads frames until @p st requests stop or capture ends,
     * - converts each @c cv::Mat to @ref frame and calls @p on_frame.
     *
     * If the stream is file-based and looping is enabled, the capture position
     * is reset to frame 0 on end-of-file.
     *
     * @param s Stream describing the source.
     * @param on_frame Callback invoked for each captured frame (frame is
     * moved).
     * @param st Stop token used for cooperative cancellation.
     */
    void daemon_start(
        const stream& s, const std::function<void(frame&&)>& on_frame,
        const std::stop_token& st
    );

    /**
     * @brief Analyze a frame and produce motion/tripwire events.
     *
     * High-level algorithm (see implementation for exact thresholds):
     * 1. Convert BGR frame to gray, blur, and diff against previous gray frame
     *    per stream.
     * 2. Threshold + morphology to obtain motion mask.
     * 3. Find contours, keep the largest, filter by minimum area and global
     *    non-zero ratio.
     * 4. Enforce a per-stream cooldown to limit event rate.
     * 5. Compute motion centroid, convert to percentage coordinates.
     * 6. If a previous centroid is available, test connected lines from the
     * stream for intersections with the motion contour and emit tripwire events
     *    respecting @ref line::dir and a per-(stream,line,direction) cooldown.
     * 7. Emit a primary motion event at centroid.
     * 8. Emit additional "bubble" motion events on a coarse grid over the mask
     *    (capped) to approximate motion shape.
     *
     * @param s Stream context (used for connected lines and naming).
     * @param f Frame to analyze.
     * @return Vector of produced events; may be empty.
     */
    std::vector<event> motion_processor(const stream& s, const frame& f);

    /**
     * @brief Create a @ref stream_manager::daemon_start_fn bound to this
     * instance.
     *
     * The returned functor forwards to @ref daemon_start.
     *
     * @return Daemon start hook for a manager.
     */
    stream_manager::daemon_start_fn daemon_start_fn();

    /**
     * @brief Create a @ref stream_manager::frame_processor_fn bound to this
     * instance.
     *
     * The returned functor forwards to @ref motion_processor.
     *
     * @return Frame processor hook for a manager.
     */
    stream_manager::frame_processor_fn frame_processor_fn();

private:
    /**
     * @brief Parse local V4L2 index from a device path.
     *
     * Accepts paths of the form "/dev/videoN".
     *
     * @param path Stream path.
     * @return Device index N, or -1 if @p path is not a local device.
     */
    int local_index_from_path(const std::string& path) const;

    /**
     * @brief Convert an OpenCV matrix to a backend frame.
     *
     * Ensures resulting frame is BGR24.
     *
     * @param m Source cv::Mat.
     * @return Converted @ref frame.
     */
    frame mat_to_frame(const cv::Mat& m) const;

    /**
     * @brief Z-component of cross product (AB x AC).
     *
     * Used for orientation tests in 2D geometry.
     */
    float cross_z(const point& a, const point& b, const point& c) const;

    /**
     * @brief Orientation of triangle (a,b,c).
     *
     * @return 1 if counter-clockwise, -1 if clockwise, 0 if collinear
     *         within @ref point::epsilon.
     */
    int orient(const point& a, const point& b, const point& c) const;

    /**
     * @brief Check whether c lies between a and b (inclusive with tolerance).
     */
    bool between(float a, float b, float c) const;

    /**
     * @brief Check whether point c lies on segment ab.
     */
    bool on_segment(const point& a, const point& b, const point& c) const;

    /**
     * @brief Test if two 2D segments intersect.
     *
     * Handles proper intersections and collinear overlaps.
     */
    bool segments_intersect(
        const point& p1, const point& p2, const point& q1, const point& q2
    ) const;

    /**
     * @brief Compute intersection point of two segments if they intersect.
     *
     * Uses parametric line intersection with epsilon checks.
     *
     * @return Intersection point in percentage coordinates, or std::nullopt.
     */
    std::optional<point> segment_intersection(
        const point& p1, const point& p2, const point& q1, const point& q2
    ) const;

    /**
     * @brief Append a motion event to the output vector.
     *
     * @param out Output event list to append to.
     * @param stream_name Source stream name.
     * @param ts Event timestamp.
     * @param pos_pct Motion position in percentage coordinates.
     */
    void add_motion_event(
        std::vector<event>& out, const std::string& stream_name,
        const std::chrono::steady_clock::time_point ts, const point& pos_pct
    ) const;

    /**
     * @brief Update best intersection candidate if current one is closer.
     *
     * Used while checking contour intersections with line segments.
     */
    void consider_hit(
        bool& hit, float& best_dist2, point& best_a, point& best_b,
        point& best_pos, const point& cur_pos_pct, const point& a,
        const point& b, const point& pos
    ) const;

    /**
     * @brief Test a single line segment against a contour polyline.
     *
     * Updates best intersection (if any) through @ref consider_hit.
     *
     * @param hit In/out: whether any hit was found.
     * @param best_dist2 In/out: best squared distance to current position.
     * @param best_a In/out: line segment start of best hit.
     * @param best_b In/out: line segment end of best hit.
     * @param best_pos In/out: intersection position for best hit.
     * @param cur_pos_pct Current motion centroid.
     * @param contour_pct Motion contour in percentage coordinates.
     * @param a Segment start.
     * @param b Segment end.
     */
    void test_line_segment_against_contour(
        bool& hit, float& best_dist2, point& best_a, point& best_b,
        point& best_pos, const point& cur_pos_pct,
        const std::vector<point>& contour_pct, const point& a, const point& b
    ) const;

    /**
     * @brief Check a motion contour against a line and emit tripwire events.
     *
     * Determines the closest intersecting segment of @p l, infers crossing
     * direction from @p prev_pos to @p cur_pos_pct, applies direction
     * constraint and cooldown, and appends a tripwire @ref event if allowed.
     *
     * @param out Output event list.
     * @param s Source stream.
     * @param l Line to test.
     * @param prev_pos Previous centroid position.
     * @param cur_pos_pct Current centroid position.
     * @param contour_pct Motion contour (percentage coordinates).
     * @param now Current timestamp.
     */
    void process_tripwire_for_line(
        std::vector<event>& out, const stream& s, const line& l,
        const point& prev_pos, const point& cur_pos_pct,
        const std::vector<point>& contour_pct,
        const std::chrono::steady_clock::time_point now
    );

    /**
     * @brief Find index of the largest OpenCV contour by area.
     *
     * @param contours Contours from OpenCV.
     * @return Index of maximum-area contour, or std::nullopt if empty.
     */
    std::optional<size_t> find_largest_contour_index(
        const std::vector<std::vector<cv::Point>>& contours
    ) const;

private:
    /** @brief Mutex guarding all per-stream state. */
    mutable std::mutex mtx;

    /**
     * @brief Previous blurred grayscale frame per stream.
     *
     * Used for frame differencing.
     */
    std::unordered_map<std::string, cv::Mat> prev_gray_by_stream;

    /**
     * @brief Last time a motion event was emitted per stream.
     *
     * Used for per-stream cooldown throttling.
     */
    std::unordered_map<std::string, std::chrono::steady_clock::time_point>
        last_emit_by_stream;

    /**
     * @brief Last known motion centroid per stream.
     *
     * Used to infer tripwire crossing direction.
     */
    std::unordered_map<std::string, point> last_pos_by_stream;

    /**
     * @brief Last time a tripwire was emitted per (stream|line|direction) key.
     *
     * Used to enforce direction-specific tripwire cooldown.
     */
    std::unordered_map<std::string, std::chrono::steady_clock::time_point>
        last_tripwire_by_key;
};

/**
 * @brief Global OpenCV daemon start wrapper.
 *
 * This function forwards to a hidden global @ref opencv_client instance.
 * It is provided for convenient use as a @ref stream_manager::daemon_start_fn.
 *
 * @param s Stream to capture.
 * @param on_frame Callback invoked with captured frames.
 * @param st Stop token.
 */
void opencv_daemon_start(
    const stream& s, const std::function<void(frame&&)>& on_frame,
    const std::stop_token& st
);

/**
 * @brief Global OpenCV motion processor wrapper.
 *
 * This function forwards to a hidden global @ref opencv_client instance.
 * It is provided for convenient use as a
 * @ref stream_manager::frame_processor_fn.
 *
 * @param s Stream context.
 * @param f Frame to analyze.
 * @return Produced events.
 */
std::vector<event> opencv_motion_processor(const stream& s, const frame& f);

} // namespace yodau::backend

#endif // YODAU_OPENCV
#endif // YODAU_BACKEND_OPENCV_CLIENT_HPP