#ifndef YODAU_BACKEND_STREAM_HPP
#define YODAU_BACKEND_STREAM_HPP

#include "geometry.hpp"

#include <mutex>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace yodau::backend {

/**
 * @brief Source/transport type of a video stream.
 *
 * This is a lightweight classification inferred from the stream path/URL
 * or specified explicitly by the user.
 *
 * @see stream::identify
 * @see stream::type_name
 */
enum stream_type {
    /** Local capture device (e.g., /dev/video*). */
    local,
    /** File-based stream (path to a video file). */
    file,
    /** RTSP network stream. */
    rtsp,
    /** HTTP/HTTPS network stream. */
    http
};

/**
 * @brief Processing pipeline mode for a stream.
 *
 * A stream can be inactive (@ref none), or active in either a user-managed
 * (@ref manual) or auto-managed (@ref automatic) pipeline.
 */
enum class stream_pipeline {
    /** Stream is active with manual/user-controlled processing. */
    manual,
    /** Stream is active with automatic backend-controlled processing. */
    automatic,
    /** Stream is not active in any pipeline. */
    none
};

/**
 * @brief Represents a single video stream and its analytic connections.
 *
 * A stream owns metadata about where it comes from (path, type),
 * whether it should loop when exhausted, and which pipeline (if any)
 * it is currently active in.
 *
 * The stream also maintains a set of connected geometric lines
 * (tripwires / ROIs), identified by their logical names.
 *
 * Thread-safety:
 * - All access to connected lines is synchronized via @ref lines_mtx.
 * - Non-line metadata (name/path/type/loop/active) is not internally
 * synchronized.
 */
class stream {
public:
    /**
     * @brief Construct a stream description.
     *
     * The actual @ref stream_type is determined as:
     * 1. Detect type from @p path using @ref identify().
     * 2. If @p type_str is empty or matches detected type name,
     *    use the detected type.
     * 3. Otherwise try to parse @p type_str as an explicit override.
     *    Unknown strings fall back to detection.
     *
     * @param path Stream path or URL.
     * @param name Logical stream name/identifier.
     * @param type_str Optional textual override ("local", "file", "rtsp",
     * "http").
     * @param loop Whether file-based streams should loop on end-of-file.
     */
    stream(
        std::string path, std::string name, const std::string& type_str = {},
        bool loop = true
    );

    /// Non-copyable (streams manage shared connections / mutex).
    stream(const stream&) = delete;
    /// Non-copyable (streams manage shared connections / mutex).
    stream& operator=(const stream&) = delete;

    /**
     * @brief Move-construct a stream.
     *
     * Moves metadata and connected lines. The moved-from object remains valid
     * but in an unspecified state.
     */
    stream(stream&& other) noexcept;

    /**
     * @brief Move-assign a stream.
     *
     * Safely swaps line connections under locks of both objects.
     *
     * @return *this.
     */
    stream& operator=(stream&& other) noexcept;

    /**
     * @brief Identify stream type from a path/URL.
     *
     * Detection rules (as implemented):
     * - "/dev/video*"  -> @ref local
     * - "rtsp://"      -> @ref rtsp
     * - "http(s)://"   -> @ref http
     * - otherwise      -> @ref file
     *
     * @param path Stream path or URL.
     * @return Detected @ref stream_type.
     */
    static stream_type identify(const std::string& path);

    /**
     * @brief Convert a stream type to a canonical textual name.
     *
     * @param type Stream type.
     * @return One of: "local", "file", "rtsp", "http", or "unknown".
     */
    static std::string type_name(const stream_type type);

    /**
     * @brief Convert a pipeline mode to its textual name.
     *
     * @param pipeline Pipeline kind.
     * @return One of: "manual", "automatic", "none", or "unknown".
     */
    static std::string pipeline_name(const stream_pipeline pipeline);

    /**
     * @brief Get logical stream name.
     */
    std::string get_name() const;

    /**
     * @brief Get stream path or URL.
     */
    std::string get_path() const;

    /**
     * @brief Get the stream transport/source type.
     */
    stream_type get_type() const;

    /**
     * @brief Whether the stream is configured to loop on exhaustion.
     *
     * Typically relevant for file streams.
     */
    bool is_looping() const;

    /**
     * @brief Dump stream metadata to an output stream.
     *
     * If @p connections is true, also prints names of any connected lines.
     *
     * @param out Output stream.
     * @param connections Whether to include connected line names.
     */
    void dump(std::ostream& out, bool connections = false) const;

    /**
     * @brief Activate the stream in a pipeline.
     *
     * This sets the current active pipeline mode.
     *
     * @param pipeline Pipeline to activate in (default: automatic).
     */
    void activate(stream_pipeline pipeline = stream_pipeline::automatic);

    /**
     * @brief Get current pipeline activity of the stream.
     *
     * @return Current @ref stream_pipeline.
     */
    stream_pipeline pipeline() const;

    /**
     * @brief Deactivate the stream.
     *
     * Sets pipeline mode to @ref stream_pipeline::none.
     */
    void deactivate();

    /**
     * @brief Connect a geometric line to this stream.
     *
     * The line is stored by its @ref line::name. If @p line is null,
     * the call is ignored.
     *
     * @param line Shared pointer to an immutable line.
     */
    void connect_line(line_ptr line);

    /**
     * @brief Get a list of names of all connected lines.
     *
     * @return Vector of line names.
     */
    std::vector<std::string> line_names() const;

    /**
     * @brief Get a snapshot of all connected lines.
     *
     * Returns a stable copy of shared pointers at the time of call.
     *
     * @return Vector of connected @ref line_ptr.
     */
    std::vector<line_ptr> lines_snapshot() const;

private:
    /** @brief Logical stream name. */
    std::string name;

    /** @brief Path or URL to the stream source. */
    std::string path;

    /** @brief Detected or user-specified stream type. */
    stream_type type;

    /** @brief Looping behavior for file streams. */
    bool loop { true };

    /** @brief Currently active pipeline mode. */
    stream_pipeline active { stream_pipeline::none };

    /**
     * @brief Connected lines keyed by their logical names.
     *
     * Protected by @ref lines_mtx.
     */
    std::unordered_map<std::string, line_ptr> lines;

    /** @brief Mutex guarding @ref lines. */
    mutable std::mutex lines_mtx;
};

} // namespace yodau::backend

#endif // YODAU_BACKEND_STREAM_HPP