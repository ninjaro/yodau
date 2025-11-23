#include "stream.hpp"

#include <ranges>

yodau::backend::stream::stream(
    std::string path, std::string name, const std::string& type_str,
    const bool loop
)
    : name(std::move(name))
    , path(std::move(path))
    , loop(loop)
    , active(stream_pipeline::none) {
    const auto detected = identify(this->path);

    if (type_str.empty() || type_str == type_name(detected)) {
        this->type = detected;
    } else if (type_str == "local") {
        this->type = stream_type::local;
    } else if (type_str == "file") {
        this->type = stream_type::file;
    } else if (type_str == "rtsp") {
        this->type = stream_type::rtsp;
    } else if (type_str == "http") {
        this->type = stream_type::http;
    } else {
        this->type = detected;
    }
}

yodau::backend::stream_type
yodau::backend::stream::identify(const std::string& path) {
    if (path.rfind("/dev/video", 0) == 0) {
        return stream_type::local;
    }
    if (path.rfind("rtsp://", 0) == 0) {
        return stream_type::rtsp;
    }
    if (path.rfind("http://", 0) == 0 || path.rfind("https://", 0) == 0) {
        return stream_type::http;
    }
    return stream_type::file;
}

std::string yodau::backend::stream::type_name(const stream_type type) {
    static constexpr std::array<std::string_view, 4> type_names {
        "local", "file", "rtsp", "http"
    };
    const auto idx = static_cast<size_t>(type);
    if (idx >= type_names.size()) {
        return "unknown";
    }
    return std::string(type_names[idx]);
}

std::string
yodau::backend::stream::pipeline_name(const stream_pipeline pipeline) {
    static constexpr std::array<std::string_view, 3> pipeline_names {
        "manual", "automatic", "none"
    };
    const auto idx = static_cast<size_t>(pipeline);
    if (idx >= pipeline_names.size()) {
        return "unknown";
    }
    return std::string(pipeline_names[idx]);
}

std::string yodau::backend::stream::get_name() const { return name; }

std::string yodau::backend::stream::get_path() const { return path; }

yodau::backend::stream_type yodau::backend::stream::get_type() const {
    return type;
}

bool yodau::backend::stream::is_looping() const { return loop; }

void yodau::backend::stream::dump(
    std::ostream& out, const bool connections
) const {
    out << "Stream(name=" << name << ", path=" << path
        << ", type=" << type_name(type)
        << ", loop=" << (loop ? "true" : "false")
        << ", active_pipeline=" << pipeline_name(active) << ")";
    if (connections && !lines.empty()) {
        out << "\n\tConnected lines:";
        for (const auto& line_name : line_names()) {
            out << ' ' << line_name;
        }
    }
}

void yodau::backend::stream::activate(const stream_pipeline pipeline) {
    active = pipeline;
}

yodau::backend::stream_pipeline yodau::backend::stream::pipeline() const {
    return active;
}

void yodau::backend::stream::deactivate() { active = stream_pipeline::none; }

void yodau::backend::stream::connect_line(line_ptr line) {
    if (!line) {
        return;
    }
    lines.emplace(line->name, line);
}

std::vector<std::string> yodau::backend::stream::line_names() const {
    return lines | std::views::keys
        | std::ranges::to<std::vector<std::string>>();
}
