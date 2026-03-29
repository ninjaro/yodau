#ifndef YODAU_BACKEND_STREAM_MANAGER_HPP
#define YODAU_BACKEND_STREAM_MANAGER_HPP
#include "streams/event.hpp"
#include "streams/frame.hpp"
#include "streams/stream.hpp"
#include <chrono>
#include <functional>
#include <mutex>
#include <stop_token>
#include <thread>
#include <unordered_map>

namespace yodau::backend {
class stream_manager {
public:
    using local_stream_detector_fn = std::function<std::vector<stream>()>;
    using manual_push_fn
        = std::function<void(const std::string& stream_name, frame&& f)>;
    using daemon_start_fn = std::function<void(
        const stream& s, std::function<void(frame&&)> on_frame,
        std::stop_token st
    )>;
    using frame_processor_fn
        = std::function<std::vector<event>(const stream& s, const frame& f)>;
    using processed_frame_sink_fn = std::function<void(
        const stream& s, const frame& f, const std::vector<event>& events
    )>;
    using event_sink_fn = std::function<void(const event& e)>;
    using event_batch_sink_fn
        = std::function<void(const std::vector<event>& events)>;

    stream_manager();

    void dump(std::ostream& out) const;
    void dump_lines(std::ostream& out) const;
    void dump_stream(std::ostream& out, bool connections = false) const;

    void set_local_stream_detector(local_stream_detector_fn detector);
    void refresh_local_streams();

    stream& add_stream(
        const std::string& path, const std::string& name = {},
        const std::string& type = {}, bool loop = true
    );
    line_ptr add_line(
        const std::string& points, bool closed = false,
        const std::string& name = {}
    );
    stream&
    set_line(const std::string& stream_name, const std::string& line_name);
    std::shared_ptr<const stream> find_stream(const std::string& name) const;

    std::vector<std::string> stream_names() const;
    std::vector<std::string> line_names() const;
    std::vector<std::string> stream_lines(const std::string& stream_name) const;

    void set_manual_push_hook(manual_push_fn hook);
    void set_daemon_start_hook(daemon_start_fn hook);

    void push_frame(const std::string& stream_name, frame&& f);
    void start_daemon(const std::string& stream_name);

    void set_frame_processor(frame_processor_fn fn);
    std::vector<event> process_frame(const std::string& stream_name, frame&& f);

    void set_processed_frame_sink(processed_frame_sink_fn fn);
    void set_event_sink(event_sink_fn fn);
    void set_event_batch_sink(event_batch_sink_fn fn);
    void set_analysis_interval_ms(int ms);
    void
    set_stream_analysis_interval_ms(const std::string& stream_name, int ms);
    void clear_stream_analysis_interval_ms(const std::string& stream_name);

    void start_stream(const std::string& name);
    void stop_stream(const std::string& name);
    bool is_stream_running(const std::string& name) const;

    void enable_fake_events(int interval_ms = 700);
    void disable_fake_events();

    void set_line_dir(const std::string& line_name, tripwire_dir dir);

private:
    std::vector<std::shared_ptr<stream>> snapshot_streams() const;
    void snapshot_hooks(
        frame_processor_fn& fp, processed_frame_sink_fn& pfs, event_sink_fn& es,
        event_batch_sink_fn& bes
    ) const;
    int
    analysis_interval_for_stream_locked(const std::string& stream_name) const;
    int current_fake_interval_ms() const;
    void run_stream_daemon(
        std::string stream_name, std::shared_ptr<stream> stream_ptr,
        daemon_start_fn daemon_fn, std::stop_token st
    );
    void run_fake_events(std::stop_token st);
    static bool is_linux_capture_ok(const stream& s);
    std::unordered_map<std::string, std::shared_ptr<stream>> streams;
    std::unordered_map<std::string, line_ptr> lines;

    size_t stream_idx { 0 };
    size_t line_idx { 0 };

    local_stream_detector_fn stream_detector {};
    manual_push_fn manual_push;
    daemon_start_fn daemon_start;
    frame_processor_fn frame_processor;
    processed_frame_sink_fn processed_frame_sink;
    event_sink_fn event_sink;
    event_batch_sink_fn event_batch_sink;

    int default_analysis_interval_ms { 200 };
    std::unordered_map<std::string, int> analysis_interval_overrides_ms;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point>
        last_analysis_ts;
    std::unordered_map<std::string, std::jthread> daemons;
    std::jthread fake_thread;
    int fake_interval_ms { 700 };
    bool fake_enabled { false };
    mutable std::mutex mtx;
};
}

#endif // YODAU_BACKEND_STREAM_MANAGER_HPP
