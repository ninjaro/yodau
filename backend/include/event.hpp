#ifndef YODAU_BACKEND_EVENT_HPP
#define YODAU_BACKEND_EVENT_HPP

#include <chrono>
#include <optional>
#include <string>

#include "geometry.hpp"

namespace yodau::backend {

/**
 * @brief High-level classification of backend events.
 *
 * Events are produced by analytics / processing modules and can represent
 * detections (e.g., motion), logical triggers (tripwire/ROI), or informational
 * messages.
 */
enum class event_kind {
    /** Motion was detected in the stream. */
    motion,
    /** A tripwire (line crossing) condition was triggered. */
    tripwire,
    /** A region-of-interest related event was triggered. */
    roi,
    /** Informational / diagnostic event that doesn't fit other categories. */
    info
};

/**
 * @brief Generic event produced by the backend.
 *
 * The event is associated with a particular stream and time. Depending on
 * @ref kind, optional spatial or semantic fields may be filled:
 * - @ref pos_pct may contain a position in **percentage coordinates**
 *   ([0.0; 100.0]) relative to frame dimensions.
 * - @ref line_name may contain a logical name for a tripwire/ROI/line that
 *   caused the event.
 *
 * @note Timestamps are stored as std::chrono::steady_clock::time_point,
 *       which is monotonic and suitable for measuring intervals, not for
 *       wall-clock time.
 */
struct event {
    /**
     * @brief Type of the event.
     *
     * Defaults to @ref event_kind::info.
     */
    event_kind kind { event_kind::info };

    /**
     * @brief Name/identifier of the stream that produced the event.
     *
     * Could be a camera ID, pipeline name, or other unique stream label.
     */
    std::string stream_name;

    /**
     * @brief Human-readable event description or payload.
     *
     * Its semantics depend on the producer (may contain JSON, plain text,
     * etc.).
     */
    std::string message;

    /**
     * @brief Monotonic timestamp when the event was generated.
     */
    std::chrono::steady_clock::time_point ts;

    /**
     * @brief Optional position associated with the event in percentage
     * coordinates.
     *
     * When present, (x, y) are expressed in percent of frame width/height.
     * For example, x=50 means horizontal center.
     */
    std::optional<point> pos_pct;

    /**
     * @brief Name of the line / ROI / rule responsible for this event.
     *
     * Used primarily for tripwire / ROI events; may be empty otherwise.
     */
    std::string line_name;
};

} // namespace yodau::backend

#endif // YODAU_BACKEND_EVENT_HPP