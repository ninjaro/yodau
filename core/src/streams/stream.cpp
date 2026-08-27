#include "streams/stream.hpp"

#include <ostream>
#include <ranges>

yodau::core::stream::stream(
    std::string stream_path, std::string stream_name,
    const std::string& type_str, const bool should_loop
)
    : name(std::move(stream_name))
    , path(std::move(stream_path))
    , loop(should_loop)
    , active(stream_pipeline::none) {
    const auto detected = identify(this->path);

    this->type = detected;
    if (type_str == "local") {
        this->type = stream_type::local;
    } else if (type_str == "file") {
        this->type = stream_type::file;
    } else if (type_str == "rtsp") {
        this->type = stream_type::rtsp;
    } else if (type_str == "http") {
        this->type = stream_type::http;
    }
}

yodau::core::stream::stream(stream&& other) noexcept
    : name(std::move(other.name))
    , path(std::move(other.path))
    , type(other.type)
    , loop(other.loop)
    , active(other.active.load()) {

    std::scoped_lock lock(other.lines_mtx);
    lines = std::move(other.lines);
    line_profiles = std::move(other.line_profiles);
}

yodau::core::stream& yodau::core::stream::operator=(stream&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    std::scoped_lock lock(lines_mtx, other.lines_mtx);

    name = std::move(other.name);
    path = std::move(other.path);
    type = other.type;
    loop = other.loop;
    active.store(other.active.load());
    lines = std::move(other.lines);
    line_profiles = std::move(other.line_profiles);

    return *this;
}

yodau::core::stream_type
yodau::core::stream::identify(const std::string& path) {
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

std::string yodau::core::stream::type_name(const stream_type type) {
    static constexpr std::array<std::string_view, 4> type_names {
        "local", "file", "rtsp", "http"
    };
    const auto idx = static_cast<size_t>(type);
    if (idx >= type_names.size()) {
        return "unknown";
    }
    return std::string(type_names[idx]);
}

std::string yodau::core::stream::pipeline_name(const stream_pipeline pipeline) {
    static constexpr std::array<std::string_view, 3> pipeline_names {
        "manual", "automatic", "none"
    };
    const auto idx = static_cast<size_t>(pipeline);
    if (idx >= pipeline_names.size()) {
        return "unknown";
    }
    return std::string(pipeline_names[idx]);
}

std::string yodau::core::stream::get_name() const { return name; }

std::string yodau::core::stream::get_path() const { return path; }

yodau::core::stream_type yodau::core::stream::get_type() const { return type; }

bool yodau::core::stream::is_looping() const { return loop; }

void yodau::core::stream::dump(
    std::ostream& out, const bool connections
) const {
    out << "Stream(name=" << name << ", path=" << path
        << ", type=" << type_name(type)
        << ", loop=" << (loop ? "true" : "false")
        << ", active_pipeline=" << pipeline_name(active.load()) << ")";

    if (!connections) {
        return;
    }

    const auto names = line_names();
    if (names.empty()) {
        return;
    }

    out << "\n\tConnected lines:";
    for (const auto& ln : names) {
        out << ' ' << ln;
    }
}

void yodau::core::stream::activate(const stream_pipeline pipeline) {
    active.store(pipeline);
}

yodau::core::stream_pipeline yodau::core::stream::pipeline() const {
    return active.load();
}

void yodau::core::stream::deactivate() { active.store(stream_pipeline::none); }

void yodau::core::stream::connect_line(
    line_ptr line, const std::optional<line_profile>& profile
) {
    if (!line) {
        return;
    }

    validate_line_geometry(line->points, line->name, line->closed);
    std::scoped_lock lock(lines_mtx);
    const auto dormant_profile = line_profiles.find(line->name);
    line_profile connected_profile = dormant_profile != line_profiles.end()
        ? dormant_profile->second
        : profile.value_or(make_line_profile(line->name));
    connected_profile.line_name = line->name;
    connected_profile.normalize();
    lines.insert_or_assign(line->name, line);
    line_profiles.insert_or_assign(line->name, connected_profile);
}

void yodau::core::stream::disconnect_line(const std::string& line_name) {
    if (line_name.empty()) {
        return;
    }

    std::scoped_lock lock(lines_mtx);
    lines.erase(line_name);
}

void yodau::core::stream::set_line_profile(line_profile profile_value) {
    if (profile_value.line_name.empty()) {
        return;
    }

    std::scoped_lock lock(lines_mtx);
    profile_value.normalize();
    line_profiles.insert_or_assign(profile_value.line_name, profile_value);
}

std::optional<yodau::core::line_profile>
yodau::core::stream::find_line_profile(const std::string& line_name) const {
    std::scoped_lock lock(lines_mtx);
    const auto profile_it = line_profiles.find(line_name);
    if (profile_it == line_profiles.end()) {
        return std::nullopt;
    }

    return profile_it->second;
}

std::vector<std::string> yodau::core::stream::line_names() const {
    std::scoped_lock lock(lines_mtx);
    return lines | std::views::keys
        | std::ranges::to<std::vector<std::string>>();
}

std::vector<yodau::core::line_ptr> yodau::core::stream::lines_snapshot() const {
    std::scoped_lock lock(lines_mtx);
    std::vector<line_ptr> out;
    out.reserve(lines.size());
    for (const auto& lp : lines | std::views::values) {
        out.push_back(lp);
    }
    return out;
}
