#include "streams/stream_manager.hpp"
#include "streams/linux_capture_device.hpp"

#include <chrono>
#include <ranges>
#include <thread>

yodau::core::stream_manager::stream_manager() { refresh_local_streams(); }

void yodau::core::stream_manager::dump(std::ostream& out) const {
    std::scoped_lock lock(mtx);
    dump_stream(out);
    out << "\n";
    dump_lines(out);
}

void yodau::core::stream_manager::dump_lines(std::ostream& out) const {
    std::scoped_lock lock(mtx);
    out << lines.size() << " lines:";
    for (const auto& line : lines | std::views::values) {
        out << "\n\t";
        line->dump(out);

        const auto profile_it = line_profiles.find(line->name);
        const line_profile profile = profile_it != line_profiles.end()
            ? profile_it->second
            : make_line_profile(line->name);
        out << " profile(width=" << profile.visual_width
            << ", interaction=" << profile.interaction_width
            << ", length=" << profile.effective_length
            << ", damping=" << profile.damping << ")";
    }
}

void yodau::core::stream_manager::dump_stream(
    std::ostream& out, const bool connections
) const {
    std::scoped_lock lock(mtx);
    out << streams.size() << " streams:";
    for (const auto& stream : streams | std::views::values) {
        out << "\n\t";
        stream->dump(out, connections);
    }
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

yodau::core::stream& yodau::core::stream_manager::add_stream(
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

yodau::core::line_ptr yodau::core::stream_manager::add_line(
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
    line_profiles.emplace(line_name, make_line_profile(line_name));
    return new_line;
}

void yodau::core::stream_manager::set_line_profile(
    line_profile profile_value
) {
    std::scoped_lock lock(mtx);

    profile_value.normalize();
    if (profile_value.line_name.empty()) {
        throw std::runtime_error("line name is required for line profile");
    }

    if (!lines.contains(profile_value.line_name)) {
        throw std::runtime_error("line not found: " + profile_value.line_name);
    }

    line_profiles[profile_value.line_name] = profile_value;
    for (const auto& stream_ptr : streams | std::views::values) {
        if (stream_ptr) {
            stream_ptr->set_line_profile(profile_value);
        }
    }
}

std::optional<yodau::core::line_profile>
yodau::core::stream_manager::find_line_profile(
    const std::string& line_name
) const {
    std::scoped_lock lock(mtx);

    if (!lines.contains(line_name)) {
        return std::nullopt;
    }

    const auto profile_it = line_profiles.find(line_name);
    if (profile_it == line_profiles.end()) {
        return make_line_profile(line_name);
    }

    return profile_it->second;
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
            throw std::runtime_error("line not found: " + profile_value.line_name);
        }

        const auto stream_it = streams.find(stream_name);
        if (stream_it == streams.end() || !stream_it->second) {
            throw std::runtime_error("stream not found: " + stream_name);
        }

        stream_ptr = stream_it->second;
    }

    if (!stream_ptr->find_line_profile(profile_value.line_name).has_value()) {
        throw std::runtime_error(
            "line not connected to stream: " + profile_value.line_name
        );
    }

    stream_ptr->set_line_profile(std::move(profile_value));
}

std::optional<yodau::core::line_profile>
yodau::core::stream_manager::find_stream_line_profile(
    const std::string& stream_name, const std::string& line_name
) const {
    std::shared_ptr<stream> stream_ptr;

    {
        std::scoped_lock lock(mtx);
        const auto stream_it = streams.find(stream_name);
        if (stream_it == streams.end()) {
            return std::nullopt;
        }

        stream_ptr = stream_it->second;
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
    const auto stream_it = streams.find(stream_name);
    if (stream_it == streams.end()) {
        throw std::runtime_error("stream not found: " + stream_name);
    }
    const auto line_it = lines.find(line_name);
    if (line_it == lines.end()) {
        throw std::runtime_error("line not found: " + line_name);
    }
    const auto profile_it = line_profiles.find(line_name);
    stream_it->second->connect_line(
        line_it->second,
        profile_it != line_profiles.end() ? std::optional<line_profile>(profile_it->second)
                                          : std::optional<line_profile>(
                                                make_line_profile(line_name)
                                            )
    );
    return *stream_it->second;
}

void yodau::core::stream_manager::clear_stream_line(
    const std::string& stream_name, const std::string& line_name
) {
    std::scoped_lock lock(mtx);
    const auto stream_it = streams.find(stream_name);
    if (stream_it == streams.end()) {
        throw std::runtime_error("stream not found: " + stream_name);
    }

    if (!lines.contains(line_name)) {
        throw std::runtime_error("line not found: " + line_name);
    }

    stream_it->second->disconnect_line(line_name);
}

std::shared_ptr<const yodau::core::stream>
yodau::core::stream_manager::find_stream(const std::string& name) const {
    std::scoped_lock lock(mtx);
    const auto it = streams.find(name);
    if (it == streams.end()) {
        return {};
    }
    return it->second;
}

std::vector<std::string> yodau::core::stream_manager::stream_names() const {
    std::scoped_lock lock(mtx);
    return streams | std::views::keys
        | std::ranges::to<std::vector<std::string>>();
}

std::vector<std::string> yodau::core::stream_manager::line_names() const {
    std::scoped_lock lock(mtx);
    return lines | std::views::keys
        | std::ranges::to<std::vector<std::string>>();
}

std::vector<std::string> yodau::core::stream_manager::stream_lines(
    const std::string& stream_name
) const {
    std::scoped_lock lock(mtx);
    const auto stream_it = streams.find(stream_name);
    if (stream_it == streams.end()) {
        return {};
    }
    return stream_it->second->line_names();
}

void yodau::core::stream_manager::set_manual_push_hook(manual_push_fn hook) {
    std::scoped_lock lock(mtx);
    manual_push = std::move(hook);
}

void yodau::core::stream_manager::set_daemon_start_hook(
    daemon_start_fn hook
) {
    std::scoped_lock lock(mtx);
    daemon_start = std::move(hook);
}

void yodau::core::stream_manager::push_frame(
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

    auto events = process_frame(stream_name, f);

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

void yodau::core::stream_manager::start_daemon(
    const std::string& stream_name
) {
    start_stream(stream_name);
}

void yodau::core::stream_manager::set_frame_processor(
    frame_processor_fn fn
) {
    std::scoped_lock lock(mtx);
    frame_processor = std::move(fn);
}

std::vector<yodau::core::event>
yodau::core::stream_manager::process_frame(
    const std::string& stream_name, const frame& f
) {
    std::shared_ptr<stream> sp;
    frame_processor_fn fp;

    {
        std::scoped_lock lock(mtx);
        auto it = streams.find(stream_name);
        if (it == streams.end() || !frame_processor) {
            return {};
        }

        fp = frame_processor;
        sp = it->second;
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
    processed_frame_sink = std::move(fn);
}

void yodau::core::stream_manager::set_event_sink(event_sink_fn fn) {
    std::scoped_lock lock(mtx);
    event_sink = std::move(fn);
}

void yodau::core::stream_manager::set_event_batch_sink(
    event_batch_sink_fn fn
) {
    std::scoped_lock lock(mtx);
    event_batch_sink = std::move(fn);
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

void yodau::core::stream_manager::stop_stream(const std::string& name) {
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

bool yodau::core::stream_manager::is_stream_running(
    const std::string& name
) const {
    std::scoped_lock lock(mtx);
    return daemons.contains(name);
}

void yodau::core::stream_manager::enable_fake_events(const int interval_ms) {
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

void yodau::core::stream_manager::disable_fake_events() {
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

void yodau::core::stream_manager::set_line_dir(
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
    const line_profile profile_value = line_profiles.contains(line_name)
        ? line_profiles.at(line_name)
        : make_line_profile(line_name);
    for (const auto& stream_ptr : streams | std::views::values) {
        if (stream_ptr && stream_ptr->find_line_profile(line_name).has_value()) {
            stream_ptr->connect_line(new_ptr, profile_value);
        }
    }
}

std::vector<std::shared_ptr<yodau::core::stream>>
yodau::core::stream_manager::snapshot_streams() const {
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

void yodau::core::stream_manager::snapshot_hooks(
    frame_processor_fn& fp, processed_frame_sink_fn& pfs, event_sink_fn& es,
    event_batch_sink_fn& bes
) const {
    std::scoped_lock lock(mtx);
    fp = frame_processor;
    pfs = processed_frame_sink;
    es = event_sink;
    bes = event_batch_sink;
}

int yodau::core::stream_manager::current_fake_interval_ms() const {
    std::scoped_lock lock(mtx);
    return fake_interval_ms;
}

void yodau::core::stream_manager::run_stream_daemon(
    std::string stream_name, std::shared_ptr<stream> stream_ptr,
    daemon_start_fn daemon_fn, std::stop_token st
) {
    daemon_fn(
        *stream_ptr,
        std::bind_front(&stream_manager::push_frame, this, stream_name), st
    );
}

void yodau::core::stream_manager::run_fake_events(std::stop_token st) {
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
