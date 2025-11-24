#ifndef YODAU_BACKEND_STREAM_MANAGER_HPP
#define YODAU_BACKEND_STREAM_MANAGER_HPP

#include "event.hpp"
#include "frame.hpp"
#include "stream.hpp"

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <ostream>
#include <stop_token>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace yodau::backend {

/**
 * @brief Central coordinator for streams, geometry, frame processing and
 * events.
 *
 * The stream_manager owns:
 * - a registry of streams (@ref stream) addressable by name,
 * - a registry of lines (@ref line) addressable by name,
 * - hooks for stream discovery, frame ingestion, background capture daemons,
 *   frame analysis, and event delivery.
 *
 * Typical responsibilities:
 * - Add and look up streams/lines.
 * - Connect lines to streams.
 * - Start/stop stream daemons (background frame producers).
 * - Accept manually pushed frames and throttle analysis per stream.
 * - Deliver produced events to configured sinks.
 *
 * Thread-safety:
 * - All public methods lock @ref mtx unless otherwise noted.
 * - Background threads (daemons and fake-event generator) also use @ref mtx.
 *
 * @note All timestamps use std::chrono::steady_clock, i.e. monotonic time.
 */
class stream_manager {
public:
    /**
     * @brief Custom detector for local streams.
     *
     * The detector is expected to return a list of streams discovered at call
     * time. Returned streams are moved into the manager if their names are not
     * already present.
     */
    using local_stream_detector_fn = std::function<std::vector<stream>()>;

    /**
     * @brief Hook for manual frame pushing.
     *
     * If set via @ref set_manual_push_hook, @ref push_frame will delegate to
     * this hook instead of analyzing frames internally.
     *
     * @param stream_name Name of the stream the frame belongs to.
     * @param f Frame to process/consume (moved).
     */
    using manual_push_fn
        = std::function<void(const std::string& stream_name, frame&& f)>;

    /**
     * @brief Hook used to start a background daemon that produces frames.
     *
     * The manager provides an on-frame callback that the daemon should call for
     * each produced frame. The daemon must also respect @p st and exit promptly
     * when stop is requested.
     *
     * @param s Stream to run.
     * @param on_frame Callback to deliver produced frames to the manager.
     * @param st Stop token to observe for cancellation.
     */
    using daemon_start_fn = std::function<void(
        const stream& s, std::function<void(frame&&)> on_frame,
        std::stop_token st
    )>;

    /**
     * @brief Frame analysis function.
     *
     * Called by @ref process_frame and (optionally) by the fake-event
     * generator.
     *
     * @param s Stream metadata/context.
     * @param f Frame to analyze (const reference).
     * @return Vector of generated events (may be empty).
     */
    using frame_processor_fn
        = std::function<std::vector<event>(const stream& s, const frame& f)>;

    /**
     * @brief Sink for individual events.
     *
     * If batch sink is not set, events are delivered one-by-one to this sink.
     */
    using event_sink_fn = std::function<void(const event& e)>;

    /**
     * @brief Sink for event batches.
     *
     * If set, @ref push_frame delivers events to this sink as a batch.
     */
    using event_batch_sink_fn
        = std::function<void(const std::vector<event>& events)>;

    /**
     * @brief Construct manager and attempt to discover local streams.
     *
     * On Linux, the constructor probes /dev/video* devices and adds those that
     * look like capture devices. After that, if a custom local detector is set,
     * it may be used when @ref refresh_local_streams is called.
     */
    stream_manager();

    /**
     * @brief Dump all streams and lines to an output stream.
     *
     * Equivalent to calling @ref dump_stream and @ref dump_lines.
     *
     * @param out Output stream.
     */
    void dump(std::ostream& out) const;

    /**
     * @brief Dump all registered lines.
     *
     * Each line is printed with @ref line::dump.
     *
     * @param out Output stream.
     */
    void dump_lines(std::ostream& out) const;

    /**
     * @brief Dump all registered streams.
     *
     * If @p connections is true, prints connected line names per stream.
     *
     * @param out Output stream.
     * @param connections Whether to include stream-line connections.
     */
    void dump_stream(std::ostream& out, bool connections = false) const;

    /**
     * @brief Set a custom local stream detector.
     *
     * The detector is stored and @ref refresh_local_streams is called
     * immediately after setting.
     *
     * @param detector Detector functor.
     */
    void set_local_stream_detector(local_stream_detector_fn detector);

    /**
     * @brief Refresh local streams.
     *
     * Behavior:
     * - On Linux: scans /dev/video* devices, validates capture capability,
     *   and auto-adds any not yet registered.
     * - If a custom detector is set: calls it and adds returned streams
     *   that are not yet registered.
     */
    void refresh_local_streams();

    /**
     * @brief Add a new stream to the manager.
     *
     * If @p name is empty or already used, a unique name "stream_N" is
     * generated.
     *
     * @param path Stream path/URL.
     * @param name Optional explicit stream name.
     * @param type Optional explicit type override passed to @ref stream ctor.
     * @param loop Whether file streams should loop on EOF.
     * @return Reference to the stored stream.
     */
    stream& add_stream(
        const std::string& path, const std::string& name = {},
        const std::string& type = {}, bool loop = true
    );

    /**
     * @brief Add a new line (polyline/polygon) to the manager.
     *
     * If @p name is empty or already used, a unique name "line_N" is generated.
     * The points string is parsed with @ref parse_points.
     *
     * @param points Textual representation of points.
     * @param closed Whether the line is closed.
     * @param name Optional explicit line name.
     * @return Shared pointer to the stored immutable line.
     * @throws std::runtime_error on invalid point string.
     */
    line_ptr add_line(
        const std::string& points, bool closed = false,
        const std::string& name = {}
    );

    /**
     * @brief Connect an existing line to an existing stream.
     *
     * @param stream_name Name of the target stream.
     * @param line_name Name of the line to connect.
     * @return Reference to the stream.
     * @throws std::runtime_error if stream or line is not found.
     */
    stream&
    set_line(const std::string& stream_name, const std::string& line_name);

    /**
     * @brief Find a stream by name.
     *
     * @param name Stream name.
     * @return Shared pointer to const stream, or empty if not found.
     */
    std::shared_ptr<const stream> find_stream(const std::string& name) const;

    /**
     * @brief List names of all registered streams.
     */
    std::vector<std::string> stream_names() const;

    /**
     * @brief List names of all registered lines.
     */
    std::vector<std::string> line_names() const;

    /**
     * @brief List names of lines connected to a given stream.
     *
     * @param stream_name Stream name.
     * @return Vector of connected line names, or empty if stream not found.
     */
    std::vector<std::string> stream_lines(const std::string& stream_name) const;

    /**
     * @brief Set manual push hook.
     *
     * When set, @ref push_frame will call this hook and return immediately,
     * skipping internal analysis and sinks.
     *
     * @param hook Hook functor (may be empty to unset).
     */
    void set_manual_push_hook(manual_push_fn hook);

    /**
     * @brief Set daemon start hook.
     *
     * This hook is required for @ref start_stream to do anything.
     *
     * @param hook Hook functor (may be empty to unset).
     */
    void set_daemon_start_hook(daemon_start_fn hook);

    /**
     * @brief Push a frame into the manager for a specific stream.
     *
     * Workflow:
     * - If manual push hook is set, delegate to it.
     * - Else analyze frame with @ref process_frame (throttled).
     * - If batch sink is set, deliver whole batch.
     * - Else if single-event sink is set, deliver events one-by-one.
     *
     * @param stream_name Stream name.
     * @param f Frame to process (moved).
     */
    void push_frame(const std::string& stream_name, frame&& f);

    /**
     * @brief Start a daemon for a stream.
     *
     * Alias for @ref start_stream.
     *
     * @param stream_name Stream name.
     */
    void start_daemon(const std::string& stream_name);

    /**
     * @brief Set the frame processor.
     *
     * @param fn Analysis functor (may be empty to unset).
     */
    void set_frame_processor(frame_processor_fn fn);

    /**
     * @brief Analyze a frame and return generated events.
     *
     * Analysis is throttled per stream by @ref analysis_interval_ms.
     * If the stream does not exist or processor is not set, returns empty list.
     *
     * @param stream_name Stream name.
     * @param f Frame to analyze (moved into function; read-only for processor).
     * @return Vector of produced events.
     */
    std::vector<event> process_frame(const std::string& stream_name, frame&& f);

    /**
     * @brief Set per-event sink.
     *
     * Ignored when batch sink is set.
     *
     * @param fn Sink functor (may be empty to unset).
     */
    void set_event_sink(event_sink_fn fn);

    /**
     * @brief Set batch event sink.
     *
     * When set, overrides per-event sink for delivery.
     *
     * @param fn Sink functor (may be empty to unset).
     */
    void set_event_batch_sink(event_batch_sink_fn fn);

    /**
     * @brief Set minimum analysis interval per stream, in milliseconds.
     *
     * Values <= 0 are ignored.
     *
     * @param ms Interval in ms.
     */
    void set_analysis_interval_ms(int ms);

    /**
     * @brief Start a stream daemon by name.
     *
     * Requirements:
     * - @ref daemon_start hook must be set,
     * - stream must exist,
     * - daemon for this stream must not already be running.
     *
     * On Linux, local capture devices may be validated again before starting.
     *
     * @param name Stream name.
     */
    void start_stream(const std::string& name);

    /**
     * @brief Stop a running stream daemon by name.
     *
     * Requests stop on the associated @ref std::jthread and deactivates the
     * stream (sets pipeline to @ref stream_pipeline::none).
     *
     * @param name Stream name.
     */
    void stop_stream(const std::string& name);

    /**
     * @brief Check whether a daemon for a stream is running.
     *
     * @param name Stream name.
     * @return true if running.
     */
    bool is_stream_running(const std::string& name) const;

    /**
     * @brief Enable periodic fake events generation.
     *
     * Starts/keeps a background thread that calls the frame processor with a
     * dummy frame on all streams every @p interval_ms milliseconds.
     *
     * @param interval_ms Period in ms (ignored if <= 0).
     */
    void enable_fake_events(int interval_ms = 700);

    /**
     * @brief Disable fake events generation.
     *
     * Requests stop on the fake-event thread (if running).
     */
    void disable_fake_events();

    /**
     * @brief Change the direction constraint of a stored line.
     *
     * Since lines are stored as immutable shared pointers, this method clones
     * the line, changes @ref line::dir, and replaces the pointer in the
     * registry.
     *
     * @param line_name Name of the line to reconfigure.
     * @param dir New direction constraint.
     * @throws std::runtime_error if the line does not exist.
     */
    void set_line_dir(const std::string& line_name, tripwire_dir dir);

private:
    /**
     * @brief Take a snapshot of current streams.
     *
     * Used to iterate without holding the manager lock for long periods.
     *
     * @return Vector of shared pointers to streams.
     */
    std::vector<std::shared_ptr<stream>> snapshot_streams() const;

    /**
     * @brief Snapshot currently installed hooks.
     *
     * Copies hooks under lock so that background threads can use stable
     * callables without racing with setters.
     *
     * @param fp Out: frame processor.
     * @param es Out: per-event sink.
     * @param bes Out: batch sink.
     */
    void snapshot_hooks(
        frame_processor_fn& fp, event_sink_fn& es, event_batch_sink_fn& bes
    ) const;

    /**
     * @brief Get current fake-event interval.
     *
     * @return Interval in milliseconds.
     */
    int current_fake_interval_ms() const;

    /**
     * @brief Background loop for fake-event generation.
     *
     * Periodically calls frame processor with a dummy frame on all streams and
     * delivers events to configured sinks until @p st requests stop.
     *
     * @param st Stop token.
     */
    void run_fake_events(std::stop_token st);

#ifdef __linux__
    /**
     * @brief Validate Linux capture devices before starting daemons.
     *
     * Local streams with /dev/video* paths may be rejected if they are not
     * usable capture devices.
     *
     * @param s Stream to validate.
     * @return true if the stream is OK to start.
     */
    static bool is_linux_capture_ok(const stream& s);
#endif

    /** @brief Registered streams keyed by name. */
    std::unordered_map<std::string, std::shared_ptr<stream>> streams;

    /** @brief Registered lines keyed by name. */
    std::unordered_map<std::string, line_ptr> lines;

    /** @brief Auto-name index for streams ("stream_N"). */
    size_t stream_idx { 0 };

    /** @brief Auto-name index for lines ("line_N"). */
    size_t line_idx { 0 };

    /** @brief Optional external local stream detector. */
    local_stream_detector_fn stream_detector {};

    /** @brief Optional manual frame push hook. */
    manual_push_fn manual_push;

    /** @brief Optional daemon start hook. */
    daemon_start_fn daemon_start;

    /** @brief Optional frame analysis hook. */
    frame_processor_fn frame_processor;

    /** @brief Optional per-event sink. */
    event_sink_fn event_sink;

    /** @brief Optional batch event sink. */
    event_batch_sink_fn event_batch_sink;

    /** @brief Per-stream analysis throttle interval. */
    int analysis_interval_ms { 200 };

    /**
     * @brief Last analysis time per stream.
     *
     * Used to enforce @ref analysis_interval_ms.
     */
    std::unordered_map<std::string, std::chrono::steady_clock::time_point>
        last_analysis_ts;

    /** @brief Running daemon threads keyed by stream name. */
    std::unordered_map<std::string, std::jthread> daemons;

    /** @brief Fake-event generator thread. */
    std::jthread fake_thread;

    /** @brief Current fake-event interval. */
    int fake_interval_ms { 700 };

    /** @brief Whether fake-event generation is enabled. */
    bool fake_enabled { false };

    /** @brief Global mutex protecting manager state. */
    mutable std::mutex mtx;
};

} // namespace yodau::backend

#endif // YODAU_BACKEND_STREAM_MANAGER_HPP