#ifndef YODAU_CORE_STREAM_MANAGER_HPP
#define YODAU_CORE_STREAM_MANAGER_HPP
#include "streams/analysis_scheduler.hpp"
#include "streams/event.hpp"
#include "streams/frame.hpp"
#include "streams/stream.hpp"
#include "streams/stream_daemon_runner.hpp"
#include "streams/stream_demo_event_runner.hpp"
#include "streams/stream_event_dispatcher.hpp"
#include "streams/stream_line_store.hpp"
#include "streams/stream_processed_frame_router.hpp"
#include "streams/stream_registry.hpp"
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace yodau::core {
class stream_manager {
public:
    using local_stream_detector_fn = std::function<std::vector<stream>()>;
    using manual_push_fn
        = std::function<void(const std::string& stream_name, frame&& f)>;
    using daemon_start_fn = stream_daemon_start_fn;
    using frame_processor_fn
        = std::function<std::vector<event>(const stream& s, const frame& f)>;
    using processed_frame_sink_fn = stream_processed_frame_sink_fn;
    using event_sink_fn = stream_event_sink_fn;
    using event_batch_sink_fn = stream_event_batch_sink_fn;
    using stream_removed_sink_fn
        = std::function<void(const std::string& stream_name)>;

    stream_manager();
    ~stream_manager();

    void shutdown();

    void dump(std::ostream& out) const;
    void dump_lines(std::ostream& out) const;
    void dump_stream(std::ostream& out, bool connections = false) const;

    void set_local_stream_detector(local_stream_detector_fn detector);
    void refresh_local_streams();
    [[nodiscard]] std::vector<std::string> detected_local_stream_names() const;

    stream& add_stream(
        const std::string& path, const std::string& name = {},
        const std::string& type = {}, bool loop = true
    );
    line_ptr add_line(
        std::vector<point> points, bool closed = false,
        const std::string& name = {}
    );
    line_ptr add_line(
        const std::string& points, bool closed = false,
        const std::string& name = {}
    );
    line_ptr upsert_line(
        std::vector<point> points, bool closed, const std::string& name
    );
    line_ptr upsert_line(
        const std::string& points, bool closed, const std::string& name
    );
    void set_line_profile(line_profile profile_value);
    std::optional<line_profile>
    find_line_profile(const std::string& line_name) const;
    std::optional<tripwire_dir>
    find_line_direction(const std::string& line_name) const;
    std::shared_ptr<const line> find_line(const std::string& line_name) const;
    void set_stream_line_profile(
        const std::string& stream_name, line_profile profile_value
    );
    std::optional<line_profile> find_stream_line_profile(
        const std::string& stream_name, const std::string& line_name
    ) const;
    stream&
    set_line(const std::string& stream_name, const std::string& line_name);
    void clear_stream_line(
        const std::string& stream_name, const std::string& line_name
    );
    std::shared_ptr<const stream> find_stream(const std::string& name) const;

    std::vector<std::string> stream_names() const;
    std::vector<std::string> line_names() const;
    std::vector<std::string> stream_lines(const std::string& stream_name) const;

    void set_manual_push_hook(manual_push_fn hook);
    void set_daemon_start_hook(daemon_start_fn hook);

    void push_frame(const std::string& stream_name, frame&& f);
    void start_daemon(const std::string& stream_name);

    void set_frame_processor(frame_processor_fn fn);
    std::vector<event>
    process_frame(const std::string& stream_name, const frame& f);

    void set_processed_frame_sink(processed_frame_sink_fn fn);
    void set_event_sink(event_sink_fn fn);
    void set_event_batch_sink(event_batch_sink_fn fn);
    void set_stream_removed_sink(stream_removed_sink_fn fn);
    void set_analysis_interval_ms(int ms);
    void
    set_stream_analysis_interval_ms(const std::string& stream_name, int ms);
    void clear_stream_analysis_interval_ms(const std::string& stream_name);

    void start_stream(const std::string& name);
    void stop_stream(const std::string& name);
    bool is_stream_running(const std::string& name) const;
    [[nodiscard]] std::optional<stream_daemon_status>
    stream_status(const std::string& name) const;

    void enable_fake_events(int interval_ms = 700);
    void disable_fake_events();

    void set_line_dir(const std::string& line_name, tripwire_dir dir);

private:
    static bool is_linux_capture_ok(const stream& s);
    void on_daemon_completed(
        const std::string& stream_name, const stream_daemon_status& status
    );
    stream_registry streams;
    stream_line_store lines;
    std::unordered_map<std::string, std::string> auto_capture_paths;

    local_stream_detector_fn stream_detector {};
    manual_push_fn manual_push;
    daemon_start_fn daemon_start;
    frame_processor_fn frame_processor;
    stream_processed_frame_router processed_frame_router;
    stream_event_dispatcher event_dispatcher;
    stream_removed_sink_fn stream_removed_sink;

    analysis_scheduler scheduler;
    stream_daemon_runner daemon_runner;
    stream_demo_event_runner demo_event_runner;
    mutable std::mutex daemon_control_mtx;
    mutable std::mutex mtx;
};
}

#endif // YODAU_CORE_STREAM_MANAGER_HPP
