#include "stream_manager.hpp"

#include <ranges>

yodau::backend::stream_manager::stream_manager() = default;

void yodau::backend::stream_manager::dump(std::ostream& out) const {
    dump_stream(out);
    out << "\n";
    dump_lines(out);
    out << "\n";
}

void yodau::backend::stream_manager::dump_lines(std::ostream& out) const {
    out << lines.size() << " lines:";
    for (const auto& line : lines | std::views::values) {
        out << "\n\t";
        line->dump(out);
    }
}

void yodau::backend::stream_manager::dump_stream(
    std::ostream& out, const bool connections
) const {
    out << streams.size() << " streams:";
    for (const auto& stream : streams | std::views::values) {
        out << "\n\t";
        stream->dump(out, connections);
    }
}

void yodau::backend::stream_manager::set_local_stream_detector(
    local_stream_detector_fn detector
) {
    stream_detector = std::move(detector);
}

void yodau::backend::stream_manager::refresh_local_streams() {
    if (!stream_detector) {
        return;
    }
    auto detected_streams = stream_detector();
    for (auto& detected_stream : detected_streams) {
        const auto name = detected_stream.get_name();
        if (!streams.contains(name)) {
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
    std::vector<point> parsed_points = parse_points(points);
    std::string line_name = name;
    while (line_name.empty() || lines.contains(line_name)) {
        line_name = "line_" + std::to_string(line_idx++);
    }
    auto new_line = make_line(std::move(parsed_points), line_name, closed);
    lines.emplace(line_name, new_line);
    return new_line;
}

void yodau::backend::stream_manager::set_line(
    const std::string& stream_name, const std::string& line_name
) {
    const auto stream_it = streams.find(stream_name);
    if (stream_it == streams.end()) {
        return;
    }
    const auto line_it = lines.find(line_name);
    if (line_it == lines.end()) {
        return;
    }
    stream_it->second->connect_line(line_it->second);
}

std::vector<std::string> yodau::backend::stream_manager::stream_names() const {
    return streams | std::views::keys
        | std::ranges::to<std::vector<std::string>>();
}

std::vector<std::string> yodau::backend::stream_manager::line_names() const {
    return lines | std::views::keys
        | std::ranges::to<std::vector<std::string>>();
}

std::vector<std::string> yodau::backend::stream_manager::stream_lines(
    const std::string& stream_name
) const {
    const auto stream_it = streams.find(stream_name);
    if (stream_it == streams.end()) {
        return {};
    }
    return stream_it->second->line_names();
}
