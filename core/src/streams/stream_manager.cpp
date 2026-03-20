#include "streams/linux_capture_device.hpp"
#include "streams/stream_manager.hpp"

#include <chrono>
#include <ranges>
#include <thread>

yodau::backend::stream_manager::stream_manager() { refresh_local_streams(); }

void yodau::backend::stream_manager::dump(std::ostream& out) const {
    std::scoped_lock lock(mtx);
    dump_stream(out);
    out << "\n";
    dump_lines(out);
}

void yodau::backend::stream_manager::dump_lines(std::ostream& out) const {
    std::scoped_lock lock(mtx);
    out << lines.size() << " lines:";
    for (const auto& line : lines | std::views::values) {
        out << "\n\t";
        line->dump(out);
    }
}

void yodau::backend::stream_manager::dump_stream(
    std::ostream& out, const bool connections
) const {
    std::scoped_lock lock(mtx);
    out << streams.size() << " streams:";
    for (const auto& stream : streams | std::views::values) {
        out << "\n\t";
        stream->dump(out, connections);
    }
}

void yodau::backend::stream_manager::set_local_stream_detector(
    local_stream_detector_fn detector
) {
    {
        std::scoped_lock lock(mtx);
        stream_detector = std::move(detector);
    }
    refresh_local_streams();
}

void yodau::backend::stream_manager::refresh_local_streams() {
    for (const std::string& path : list_linux_capture_devices()) {
        const std::string stream_name = "video" + path.substr(10);
        {
            std::scoped_lock lock(mtx);
            if (streams.contains(stream_name)) {
                continue;
            }
        }

        add_stream(path, stream_name, "local");
    }

    local_stream_detector_fn det;
    {
        std::scoped_lock lock(mtx);
        det = stream_detector;
    }

    if (!det) {
        return;
    }

    auto detected_streams = det();
    for (auto& detected_stream : detected_streams) {
        const auto name = detected_stream.get_name();

        std::scoped_lock lock(mtx);
        if (!streams.contains(name)) { // todo: update existing streams?
            streams.emplace(
                name, std::make_shared<stream>(std::move(detected_stream))
            );
        }
    }
}

yodau::backend::stream& yodau::backend::stream_manager::add_stream(
    const std::string& path, const std::string& name, const std::string& type,
    bool loop
) {
    std::scoped_lock lock(mtx);
    std::string stream_name = name;
    while (stream_name.empty() || streams.contains(stream_name)) {
        stream_name = "stream_" + std::to_string(stream_idx++);
    }
    auto new_stream = std::make_shared<stream>(path, stream_name, type, loop);
    auto& ref = *new_stream;
    streams.emplace(stream_name, std::move(new_stream));
    return ref;
}

yodau::backend::line_ptr yodau::backend::stream_manager::add_line(
    const std::string& points, const bool closed, const std::string& name
) {
    std::scoped_lock lock(mtx);
    std::vector<point> parsed_points = parse_points(points);
    std::string line_name = name;
    while (line_name.empty() || lines.contains(line_name)) {
        line_name = "line_" + std::to_string(line_idx++);
    }
    auto new_line = make_line(std::move(parsed_points), line_name, closed);
    lines.emplace(line_name, new_line);
    return new_line;
}

yodau::backend::stream& yodau::backend::stream_manager::set_line(
    const std::string& stream_name, const std::string& line_name
) {
    std::scoped_lock lock(mtx);
    const auto stream_it = streams.find(stream_name);
    if (stream_it == streams.end()) {
        throw std::runtime_error("stream not found: " + stream_name);
    }
    const auto line_it = lines.find(line_name);
    if (line_it == lines.end()) {
        throw std::runtime_error("line not found: " + line_name);
    }
    stream_it->second->connect_line(line_it->second);
    return *stream_it->second;
}

std::shared_ptr<const yodau::backend::stream>
yodau::backend::stream_manager::find_stream(const std::string& name) const {
    std::scoped_lock lock(mtx);
    const auto it = streams.find(name);
    if (it == streams.end()) {
        return {};
    }
    return it->second;
}

std::vector<std::string> yodau::backend::stream_manager::stream_names() const {
    std::scoped_lock lock(mtx);
    return streams | std::views::keys
        | std::ranges::to<std::vector<std::string>>();
}

std::vector<std::string> yodau::backend::stream_manager::line_names() const {
    std::scoped_lock lock(mtx);
    return lines | std::views::keys
        | std::ranges::to<std::vector<std::string>>();
}

std::vector<std::string> yodau::backend::stream_manager::stream_lines(
    const std::string& stream_name
) const {
    std::scoped_lock lock(mtx);
    const auto stream_it = streams.find(stream_name);
    if (stream_it == streams.end()) {
        return {};
    }
    return stream_it->second->line_names();
}

void yodau::backend::stream_manager::set_manual_push_hook(manual_push_fn hook) {
    std::scoped_lock lock(mtx);
    manual_push = std::move(hook);
}

void yodau::backend::stream_manager::set_daemon_start_hook(
    daemon_start_fn hook
) {
    std::scoped_lock lock(mtx);
    daemon_start = std::move(hook);
}

void yodau::backend::stream_manager::push_frame(
    const std::string& stream_name, frame&& f
) {
    manual_push_fn mp;
    processed_frame_sink_fn pfs;
    event_sink_fn es;
    event_batch_sink_fn bes;
    std::shared_ptr<stream> sp;

    {
        std::scoped_lock lock(mtx);
        mp = manual_push;
        pfs = processed_frame_sink;
        es = event_sink;
        bes = event_batch_sink;

        const auto stream_it = streams.find(stream_name);
        if (stream_it != streams.end()) {
            sp = stream_it->second;
        }
    }

    if (mp) {
        mp(stream_name, std::move(f));
        return;
    }

    auto events = process_frame(stream_name, std::move(f));

    if (pfs && sp) {
        pfs(*sp, f, events);
    }

    if (bes) {
        bes(events);
        return;
    }

    if (!es) {
        return;
    }

    for (const auto& e : events) {
        es(e);
    }
}

void yodau::backend::stream_manager::start_daemon(
    const std::string& stream_name
) {
    start_stream(stream_name);
}

void yodau::backend::stream_manager::set_frame_processor(
    frame_processor_fn fn
) {
    std::scoped_lock lock(mtx);
    frame_processor = std::move(fn);
}

std::vector<yodau::backend::event>
yodau::backend::stream_manager::process_frame(
    const std::string& stream_name, frame&& f
) {
    std::shared_ptr<stream> sp;
    frame_processor_fn fp;
    const auto now = std::chrono::steady_clock::now();
    bool allow = false;

    {
        std::scoped_lock lock(mtx);
        auto it = streams.find(stream_name);
        if (it == streams.end() || !frame_processor) {
            return {};
        }

        fp = frame_processor;
        sp = it->second;

        const auto last_it = last_analysis_ts.find(stream_name);
        if (last_it == last_analysis_ts.end()) {
            last_analysis_ts[stream_name] = now;
            allow = true;
        } else {
            const auto dt
                = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now - last_it->second
                )
                      .count();
            if (dt >= analysis_interval_ms) {
                last_analysis_ts[stream_name] = now;
                allow = true;
            }
        }
    }

    if (!allow || !sp || !fp) {
        return {};
    }

    return fp(*sp, f);
}

void yodau::backend::stream_manager::set_processed_frame_sink(
    processed_frame_sink_fn fn
) {
    std::scoped_lock lock(mtx);
    processed_frame_sink = std::move(fn);
}

void yodau::backend::stream_manager::set_event_sink(event_sink_fn fn) {
    std::scoped_lock lock(mtx);
    event_sink = std::move(fn);
}

void yodau::backend::stream_manager::set_event_batch_sink(
    event_batch_sink_fn fn
) {
    std::scoped_lock lock(mtx);
    event_batch_sink = std::move(fn);
}

void yodau::backend::stream_manager::set_analysis_interval_ms(int ms) {
    if (ms <= 0) {
        return;
    }
    std::scoped_lock lock(mtx);
    analysis_interval_ms = ms;
}

void yodau::backend::stream_manager::start_stream(const std::string& name) {
    std::shared_ptr<stream> sp;
    daemon_start_fn ds;

    {
        std::scoped_lock lock(mtx);
        if (!daemon_start || daemons.contains(name)) {
            return;
        }

        const auto it = streams.find(name);
        if (it == streams.end() || !it->second) {
            return;
        }

        sp = it->second;
        ds = daemon_start;

        if (!is_linux_capture_ok(*sp)) {
            return;
        }

        sp->activate(stream_pipeline::automatic);
    }

    auto daemon_runner = std::bind_front(
        &stream_manager::run_stream_daemon, this, name, sp, ds
    );
    std::jthread th(std::move(daemon_runner));

    {
        std::scoped_lock lock(mtx);
        daemons.emplace(name, std::move(th));
    }
}

void yodau::backend::stream_manager::stop_stream(const std::string& name) {
    std::jthread th;
    std::shared_ptr<stream> sp;

    {
        std::scoped_lock lock(mtx);
        const auto it = daemons.find(name);
        if (it == daemons.end()) {
            return;
        }

        th = std::move(it->second);
        daemons.erase(it);

        const auto sit = streams.find(name);
        if (sit != streams.end()) {
            sp = sit->second;
        }
    }

    th.request_stop();

    if (sp) {
        std::scoped_lock lock(mtx);
        sp->deactivate();
    }
}

bool yodau::backend::stream_manager::is_stream_running(
    const std::string& name
) const {
    std::scoped_lock lock(mtx);
    return daemons.contains(name);
}

void yodau::backend::stream_manager::enable_fake_events(const int interval_ms) {
    {
        std::scoped_lock lock(mtx);

        if (interval_ms > 0) {
            fake_interval_ms = interval_ms;
        }

        if (fake_enabled) {
            return;
        }

        fake_enabled = true;
    }

    auto fake_runner = std::bind_front(&stream_manager::run_fake_events, this);
    std::jthread th(std::move(fake_runner));

    {
        std::scoped_lock lock(mtx);
        if (!fake_enabled) {
            th.request_stop();
            return;
        }
        fake_thread = std::move(th);
    }
}

void yodau::backend::stream_manager::disable_fake_events() {
    std::jthread th;

    {
        std::scoped_lock lock(mtx);

        if (!fake_enabled) {
            return;
        }

        fake_enabled = false;
        th = std::move(fake_thread);
        fake_thread = std::jthread();
    }

    if (th.joinable()) {
        th.request_stop();
    }
}

void yodau::backend::stream_manager::set_line_dir(
    const std::string& line_name, tripwire_dir dir
) {
    std::scoped_lock lock(mtx);

    const auto it = lines.find(line_name);
    if (it == lines.end() || !it->second) {
        throw std::runtime_error("line not found: " + line_name);
    }

    const auto old_ptr = it->second;
    auto new_ptr = std::make_shared<line>(*old_ptr);
    new_ptr->dir = dir;

    it->second = new_ptr;
}

std::vector<std::shared_ptr<yodau::backend::stream>>
yodau::backend::stream_manager::snapshot_streams() const {
    std::vector<std::shared_ptr<stream>> snap;

    std::scoped_lock lock(mtx);
    snap.reserve(streams.size());
    for (auto& sp : streams | std::views::values) {
        if (sp) {
            snap.push_back(sp);
        }
    }

    return snap;
}

void yodau::backend::stream_manager::snapshot_hooks(
    frame_processor_fn& fp, processed_frame_sink_fn& pfs, event_sink_fn& es,
    event_batch_sink_fn& bes
) const {
    std::scoped_lock lock(mtx);
    fp = frame_processor;
    pfs = processed_frame_sink;
    es = event_sink;
    bes = event_batch_sink;
}

int yodau::backend::stream_manager::current_fake_interval_ms() const {
    std::scoped_lock lock(mtx);
    return fake_interval_ms;
}

void yodau::backend::stream_manager::run_stream_daemon(
    std::string stream_name, std::shared_ptr<stream> stream_ptr,
    daemon_start_fn daemon_fn, std::stop_token st
) {
    daemon_fn(
        *stream_ptr,
        std::bind_front(&stream_manager::push_frame, this, stream_name), st
    );
}

void yodau::backend::stream_manager::run_fake_events(std::stop_token st) {
    frame dummy;

    while (!st.stop_requested()) {
        auto snap = snapshot_streams();

        frame_processor_fn fp;
        processed_frame_sink_fn pfs;
        event_sink_fn es;
        event_batch_sink_fn bes;
        snapshot_hooks(fp, pfs, es, bes);

        if (fp) {
            for (const auto& sp : snap) {
                auto evs = fp(*sp, dummy);

                if (pfs) {
                    pfs(*sp, dummy, evs);
                }

                if (bes) {
                    if (!evs.empty()) {
                        bes(evs);
                    }
                } else if (es) {
                    for (const auto& e : evs) {
                        es(e);
                    }
                }
            }
        }

        const int interval = current_fake_interval_ms();
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
    }
}

bool yodau::backend::stream_manager::is_linux_capture_ok(const stream& s) {
    if (s.get_type() != local) {
        return true;
    }

    const auto& p = s.get_path();
    if (p.rfind("/dev/video", 0) != 0) {
        return true;
    }

    return is_linux_capture_device(p);
}
