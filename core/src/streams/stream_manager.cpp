#include "streams/stream_manager.hpp"
#include "streams/linux_capture_device.hpp"

#include <algorithm>
#include <filesystem>
#include <ostream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

yodau::core::stream_manager::stream_manager() { refresh_local_streams(); }

yodau::core::stream_manager::~stream_manager() { shutdown(); }

void yodau::core::stream_manager::shutdown() {
    disable_fake_events();
    std::scoped_lock daemon_control_lock(daemon_control_mtx);
    daemon_runner.stop_all();

    std::scoped_lock lock(mtx);
    for (const auto& stream_ptr : streams.snapshot()) {
        if (stream_ptr) {
            stream_ptr->deactivate();
        }
    }

    manual_push = {};
    daemon_start = {};
    frame_processor = {};
    processed_frame_router.set_sink({});
    event_dispatcher.set_event_sink({});
    event_dispatcher.set_event_batch_sink({});
    stream_removed_sink = {};
}

void yodau::core::stream_manager::dump(std::ostream& out) const {
    std::scoped_lock lock(mtx);
    streams.dump(out);
    out << "\n";
    lines.dump(out);
}

void yodau::core::stream_manager::dump_lines(std::ostream& out) const {
    std::scoped_lock lock(mtx);
    lines.dump(out);
}

void yodau::core::stream_manager::dump_stream(
    std::ostream& out, const bool connections
) const {
    std::scoped_lock lock(mtx);
    streams.dump(out, connections);
}

void yodau::core::stream_manager::set_local_stream_detector(
    local_stream_detector_fn detector
) {
    {
        std::scoped_lock lock(mtx);
        stream_detector = std::move(detector);
    }
    refresh_local_streams();
}

void yodau::core::stream_manager::refresh_local_streams() {
    local_stream_detector_fn detector;
    {
        std::scoped_lock lock(mtx);
        detector = stream_detector;
    }

    std::vector<stream> detected_streams;
    if (detector) {
        detected_streams = detector();
    } else {
        for (const std::string& path : list_linux_capture_devices()) {
            const std::string filename
                = std::filesystem::path(path).filename().string();
            detected_streams.emplace_back(path, filename, "local", false);
        }
    }

    std::unordered_map<std::string, std::string> detected_capture_paths;
    for (const stream& detected : detected_streams) {
        detected_capture_paths.insert_or_assign(
            detected.get_name(), detected.get_path()
        );
    }

    std::vector<std::string> disappeared_streams;
    {
        std::scoped_lock lock(mtx);
        for (const auto& [stream_name, path] : auto_capture_paths) {
            const auto detected_it = detected_capture_paths.find(stream_name);
            if (detected_it == detected_capture_paths.end()
                || detected_it->second != path) {
                disappeared_streams.push_back(stream_name);
            }
        }
    }

    for (const std::string& stream_name : disappeared_streams) {
        stop_stream(stream_name);
        stream_removed_sink_fn removed_sink;
        bool removed = false;
        {
            std::scoped_lock lock(mtx);
            const auto tracked_it = auto_capture_paths.find(stream_name);
            const auto detected_it = detected_capture_paths.find(stream_name);
            if (tracked_it != auto_capture_paths.end()
                && (detected_it == detected_capture_paths.end()
                    || detected_it->second != tracked_it->second)) {
                streams.erase(stream_name);
                auto_capture_paths.erase(tracked_it);
                removed_sink = stream_removed_sink;
                removed = true;
            }
        }
        if (removed) {
            scheduler.remove_stream(stream_name);
            if (removed_sink) {
                removed_sink(stream_name);
            }
        }
    }

    {
        std::scoped_lock lock(mtx);
        for (auto& detected : detected_streams) {
            const std::string stream_name = detected.get_name();
            const std::string path = detected.get_path();
            const auto selected = detected_capture_paths.find(stream_name);
            if (stream_name.empty() || path.empty()
                || selected == detected_capture_paths.end()
                || selected->second != path || streams.contains(stream_name)) {
                continue;
            }
            streams.add_detected(std::move(detected));
            auto_capture_paths.insert_or_assign(stream_name, path);
        }
    }
}

std::vector<std::string>
yodau::core::stream_manager::detected_local_stream_names() const {
    std::scoped_lock lock(mtx);
    std::vector<std::string> names;
    names.reserve(auto_capture_paths.size());
    for (const auto& entry : auto_capture_paths) {
        names.push_back(entry.first);
    }
    std::ranges::sort(names);
    return names;
}

yodau::core::stream& yodau::core::stream_manager::add_stream(
    const std::string& path, const std::string& name, const std::string& type,
    bool loop
) {
    std::scoped_lock lock(mtx);
    return streams.add(path, name, type, loop);
}

yodau::core::line_ptr yodau::core::stream_manager::add_line(
    const std::string& points, const bool closed, const std::string& name
) {
    return add_line(parse_points(points), closed, name);
}

yodau::core::line_ptr yodau::core::stream_manager::add_line(
    std::vector<point> points, const bool closed, const std::string& name
) {
    std::scoped_lock lock(mtx);
    return lines.add(std::move(points), closed, name);
}

yodau::core::line_ptr yodau::core::stream_manager::upsert_line(
    const std::string& points, const bool closed, const std::string& name
) {
    return upsert_line(parse_points(points), closed, name);
}

yodau::core::line_ptr yodau::core::stream_manager::upsert_line(
    std::vector<point> points, const bool closed, const std::string& name
) {
    std::scoped_lock lock(mtx);
    const line_ptr replacement = lines.upsert(std::move(points), closed, name);
    for (const auto& stream_ptr : streams.snapshot()) {
        if (!stream_ptr) {
            continue;
        }
        const auto connected_names = stream_ptr->line_names();
        if (std::ranges::find(connected_names, name) == connected_names.end()) {
            continue;
        }
        stream_ptr->connect_line(
            replacement, stream_ptr->find_line_profile(name)
        );
    }
    return replacement;
}

void yodau::core::stream_manager::set_line_profile(line_profile profile_value) {
    std::scoped_lock lock(mtx);
    lines.set_profile(std::move(profile_value));
}

std::optional<yodau::core::line_profile>
yodau::core::stream_manager::find_line_profile(
    const std::string& line_name
) const {
    std::scoped_lock lock(mtx);
    return lines.find_profile(line_name);
}

std::optional<yodau::core::tripwire_dir>
yodau::core::stream_manager::find_line_direction(
    const std::string& line_name
) const {
    std::scoped_lock lock(mtx);
    return lines.find_direction(line_name);
}

std::shared_ptr<const yodau::core::line>
yodau::core::stream_manager::find_line(const std::string& line_name) const {
    std::scoped_lock lock(mtx);
    return lines.find(line_name);
}

void yodau::core::stream_manager::set_stream_line_profile(
    const std::string& stream_name, line_profile profile_value
) {
    std::shared_ptr<stream> stream_ptr;

    {
        std::scoped_lock lock(mtx);

        profile_value.normalize();
        if (profile_value.line_name.empty()) {
            throw std::runtime_error("line name is required for line profile");
        }

        if (!lines.contains(profile_value.line_name)) {
            throw std::runtime_error(
                "line not found: " + profile_value.line_name
            );
        }

        stream_ptr = streams.find(stream_name);
        if (!stream_ptr) {
            throw std::runtime_error("stream not found: " + stream_name);
        }
    }

    // A disabled imported line still needs a per-stream profile so enabling it
    // later restores exactly the settings saved by the Qt application.
    stream_ptr->set_line_profile(std::move(profile_value));
}

std::optional<yodau::core::line_profile>
yodau::core::stream_manager::find_stream_line_profile(
    const std::string& stream_name, const std::string& line_name
) const {
    std::shared_ptr<stream> stream_ptr;

    {
        std::scoped_lock lock(mtx);
        stream_ptr = streams.find(stream_name);
    }

    if (!stream_ptr) {
        return std::nullopt;
    }

    return stream_ptr->find_line_profile(line_name);
}

yodau::core::stream& yodau::core::stream_manager::set_line(
    const std::string& stream_name, const std::string& line_name
) {
    std::scoped_lock lock(mtx);
    const auto stream_ptr = streams.find(stream_name);
    if (!stream_ptr) {
        throw std::runtime_error("stream not found: " + stream_name);
    }

    const auto connection = lines.connection(line_name);
    stream_ptr->connect_line(connection.line, connection.profile);
    return *stream_ptr;
}

void yodau::core::stream_manager::clear_stream_line(
    const std::string& stream_name, const std::string& line_name
) {
    std::scoped_lock lock(mtx);
    const auto stream_ptr = streams.find(stream_name);
    if (!stream_ptr) {
        throw std::runtime_error("stream not found: " + stream_name);
    }

    if (!lines.contains(line_name)) {
        throw std::runtime_error("line not found: " + line_name);
    }

    stream_ptr->disconnect_line(line_name);
}

std::shared_ptr<const yodau::core::stream>
yodau::core::stream_manager::find_stream(const std::string& name) const {
    std::scoped_lock lock(mtx);
    return streams.find_const(name);
}

std::vector<std::string> yodau::core::stream_manager::stream_names() const {
    std::scoped_lock lock(mtx);
    return streams.names();
}

std::vector<std::string> yodau::core::stream_manager::line_names() const {
    std::scoped_lock lock(mtx);
    return lines.names();
}

std::vector<std::string> yodau::core::stream_manager::stream_lines(
    const std::string& stream_name
) const {
    std::scoped_lock lock(mtx);
    const auto stream_ptr = streams.find(stream_name);
    if (!stream_ptr) {
        return {};
    }
    return stream_ptr->line_names();
}

void yodau::core::stream_manager::set_manual_push_hook(manual_push_fn hook) {
    std::scoped_lock lock(mtx);
    manual_push = std::move(hook);
}

void yodau::core::stream_manager::set_daemon_start_hook(daemon_start_fn hook) {
    std::scoped_lock lock(mtx);
    daemon_start = std::move(hook);
}

void yodau::core::stream_manager::push_frame(
    const std::string& stream_name, frame&& f
) {
    manual_push_fn mp;
    processed_frame_sink_fn pfs;
    stream_event_sinks event_sinks;
    std::shared_ptr<stream> sp;

    {
        std::scoped_lock lock(mtx);
        mp = manual_push;
        pfs = processed_frame_router.snapshot();
        event_sinks = event_dispatcher.snapshot();

        sp = streams.find(stream_name);
    }

    if (mp) {
        mp(stream_name, std::move(f));
        return;
    }

    auto events = process_frame(stream_name, f);

    if (pfs && sp) {
        pfs(*sp, f, events);
    }

    event_sinks.dispatch(events);
}

void yodau::core::stream_manager::start_daemon(const std::string& stream_name) {
    start_stream(stream_name);
}

void yodau::core::stream_manager::set_frame_processor(frame_processor_fn fn) {
    std::scoped_lock lock(mtx);
    frame_processor = std::move(fn);
}

std::vector<yodau::core::event> yodau::core::stream_manager::process_frame(
    const std::string& stream_name, const frame& f
) {
    std::shared_ptr<stream> sp;
    frame_processor_fn fp;

    {
        std::scoped_lock lock(mtx);
        if (!frame_processor) {
            return {};
        }

        fp = frame_processor;
        sp = streams.find(stream_name);
    }

    if (!sp || !fp || !scheduler.should_process(stream_name)) {
        return {};
    }

    return fp(*sp, f);
}

void yodau::core::stream_manager::set_processed_frame_sink(
    processed_frame_sink_fn fn
) {
    std::scoped_lock lock(mtx);
    processed_frame_router.set_sink(std::move(fn));
}

void yodau::core::stream_manager::set_event_sink(event_sink_fn fn) {
    std::scoped_lock lock(mtx);
    event_dispatcher.set_event_sink(std::move(fn));
}

void yodau::core::stream_manager::set_event_batch_sink(event_batch_sink_fn fn) {
    std::scoped_lock lock(mtx);
    event_dispatcher.set_event_batch_sink(std::move(fn));
}

void yodau::core::stream_manager::set_stream_removed_sink(
    stream_removed_sink_fn fn
) {
    std::scoped_lock lock(mtx);
    stream_removed_sink = std::move(fn);
}

void yodau::core::stream_manager::set_analysis_interval_ms(int ms) {
    scheduler.set_default_interval_ms(ms);
}

void yodau::core::stream_manager::set_stream_analysis_interval_ms(
    const std::string& stream_name, const int ms
) {
    scheduler.set_stream_interval_ms(stream_name, ms);
}

void yodau::core::stream_manager::clear_stream_analysis_interval_ms(
    const std::string& stream_name
) {
    scheduler.clear_stream_interval_ms(stream_name);
}

void yodau::core::stream_manager::start_stream(const std::string& name) {
    std::scoped_lock daemon_control_lock(daemon_control_mtx);

    if (daemon_runner.is_running(name)) {
        return;
    }
    // Reap a naturally completed/failed entry before activating its successor.
    // This joins the old completion callback without holding the manager mutex.
    (void)daemon_runner.stop(name);

    std::shared_ptr<stream> sp;
    daemon_start_fn ds;

    {
        std::scoped_lock lock(mtx);
        if (!daemon_start) {
            return;
        }

        sp = streams.find(name);
        if (!sp) {
            return;
        }

        ds = daemon_start;

        if (!is_linux_capture_ok(*sp)) {
            return;
        }

        sp->activate(stream_pipeline::automatic);

        const bool started = daemon_runner.start(
            name, sp, ds,
            [this](const std::string& stream_name, frame&& f) {
                push_frame(stream_name, std::move(f));
            },
            [this](
                const std::string& stream_name,
                const stream_daemon_status& status
            ) { on_daemon_completed(stream_name, status); }
        );
        if (!started) {
            sp->deactivate();
        }
    }
}

void yodau::core::stream_manager::stop_stream(const std::string& name) {
    std::scoped_lock daemon_control_lock(daemon_control_mtx);
    std::shared_ptr<stream> sp;

    {
        std::scoped_lock lock(mtx);
        if (!daemon_runner.is_running(name)) {
            return;
        }

        sp = streams.find(name);
    }

    if (!daemon_runner.stop(name)) {
        return;
    }

    if (sp) {
        std::scoped_lock lock(mtx);
        if (!daemon_runner.is_running(name)) {
            sp->deactivate();
        }
    }
}

bool yodau::core::stream_manager::is_stream_running(
    const std::string& name
) const {
    return daemon_runner.is_running(name);
}

std::optional<yodau::core::stream_daemon_status>
yodau::core::stream_manager::stream_status(const std::string& name) const {
    return daemon_runner.status(name);
}

void yodau::core::stream_manager::on_daemon_completed(
    const std::string& stream_name, const stream_daemon_status& status
) {
    (void)status;
    std::shared_ptr<stream> stream_ptr;
    {
        std::scoped_lock lock(mtx);
        stream_ptr = streams.find(stream_name);
    }
    if (stream_ptr) {
        stream_ptr->deactivate();
    }
}

void yodau::core::stream_manager::enable_fake_events(const int interval_ms) {
    demo_event_runner.start(
        interval_ms,
        [this]() {
            std::scoped_lock lock(mtx);
            return streams.snapshot();
        },
        [this]() {
            std::scoped_lock lock(mtx);
            return stream_demo_hooks {
                .frame_processor = frame_processor,
                .processed_frame_sink = processed_frame_router.snapshot(),
                .event_sinks = event_dispatcher.snapshot(),
            };
        }
    );
}

void yodau::core::stream_manager::disable_fake_events() {
    demo_event_runner.stop();
}

void yodau::core::stream_manager::set_line_dir(
    const std::string& line_name, tripwire_dir dir
) {
    std::scoped_lock lock(mtx);

    const auto connection = lines.set_direction(line_name, dir);
    for (const auto& stream_ptr : streams.snapshot()) {
        if (!stream_ptr) {
            continue;
        }
        const auto connected_lines = stream_ptr->line_names();
        if (std::ranges::find(connected_lines, line_name)
            != connected_lines.end()) {
            stream_ptr->connect_line(
                connection.line, stream_ptr->find_line_profile(line_name)
            );
        }
    }
}

bool yodau::core::stream_manager::is_linux_capture_ok(const stream& s) {
    if (s.get_type() != local) {
        return true;
    }

    const auto& p = s.get_path();
    if (p.rfind("/dev/video", 0) != 0) {
        return true;
    }

    return is_linux_capture_device(p);
}
