#include "streams/stream_line_store.hpp"

#include <memory>
#include <ostream>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace yodau::core {

line_ptr stream_line_store::add(
    const std::string& points, const bool closed, const std::string& name
) {
    return add(parse_points(points), closed, name);
}

line_ptr stream_line_store::add(
    std::vector<point> points, const bool closed, const std::string& name
) {
    std::string line_name = name;
    while (line_name.empty() || lines_.contains(line_name)) {
        line_name = "line_" + std::to_string(line_idx_++);
    }

    validate_line_geometry(points, line_name, closed);
    auto new_line = make_line(std::move(points), line_name, closed);
    lines_.emplace(line_name, new_line);
    line_profiles_.emplace(line_name, make_line_profile(line_name));
    return new_line;
}

line_ptr stream_line_store::upsert(
    const std::string& points, const bool closed, const std::string& name
) {
    return upsert(parse_points(points), closed, name);
}

line_ptr stream_line_store::upsert(
    std::vector<point> points, const bool closed, const std::string& name
) {
    if (name.empty()) {
        throw std::invalid_argument("line name is required for upsert");
    }
    validate_line_geometry(points, name, closed);
    auto replacement = make_line(std::move(points), name, closed);

    if (const auto existing = lines_.find(name);
        existing != lines_.end() && existing->second) {
        auto replacement_with_direction = std::make_shared<line>(*replacement);
        replacement_with_direction->dir = existing->second->dir;
        replacement = std::move(replacement_with_direction);
        existing->second = replacement;
    } else {
        lines_.insert_or_assign(name, replacement);
        line_profiles_.try_emplace(name, make_line_profile(name));
    }
    return replacement;
}

line_profile stream_line_store::set_profile(line_profile profile_value) {
    profile_value.normalize();
    if (profile_value.line_name.empty()) {
        throw std::runtime_error("line name is required for line profile");
    }

    if (!lines_.contains(profile_value.line_name)) {
        throw std::runtime_error("line not found: " + profile_value.line_name);
    }

    line_profiles_[profile_value.line_name] = profile_value;
    return profile_value;
}

std::optional<line_profile>
stream_line_store::find_profile(const std::string& line_name) const {
    if (!lines_.contains(line_name)) {
        return std::nullopt;
    }

    const auto profile_it = line_profiles_.find(line_name);
    if (profile_it == line_profiles_.end()) {
        return make_line_profile(line_name);
    }

    return profile_it->second;
}

std::optional<tripwire_dir>
stream_line_store::find_direction(const std::string& line_name) const {
    const auto line_it = lines_.find(line_name);
    if (line_it == lines_.end() || !line_it->second) {
        return std::nullopt;
    }
    return line_it->second->dir;
}

stream_line_connection
stream_line_store::connection(const std::string& line_name) const {
    const auto line_it = lines_.find(line_name);
    if (line_it == lines_.end() || !line_it->second) {
        throw std::runtime_error("line not found: " + line_name);
    }

    const auto profile
        = find_profile(line_name).value_or(make_line_profile(line_name));
    return stream_line_connection {
        .line = line_it->second,
        .profile = profile,
    };
}

bool stream_line_store::contains(const std::string& line_name) const {
    return lines_.contains(line_name);
}

line_ptr stream_line_store::find(const std::string& line_name) const {
    const auto line_it = lines_.find(line_name);
    return line_it == lines_.end() ? line_ptr {} : line_it->second;
}

std::vector<std::string> stream_line_store::names() const {
    return lines_ | std::views::keys
        | std::ranges::to<std::vector<std::string>>();
}

size_t stream_line_store::size() const { return lines_.size(); }

stream_line_connection stream_line_store::set_direction(
    const std::string& line_name, const tripwire_dir dir
) {
    const auto line_it = lines_.find(line_name);
    if (line_it == lines_.end() || !line_it->second) {
        throw std::runtime_error("line not found: " + line_name);
    }

    auto new_line = std::make_shared<line>(*line_it->second);
    new_line->dir = dir;
    line_it->second = new_line;

    return stream_line_connection {
        .line = new_line,
        .profile
        = find_profile(line_name).value_or(make_line_profile(line_name)),
    };
}

void stream_line_store::dump(std::ostream& out) const {
    out << lines_.size() << " lines:";
    for (const auto& line_value : lines_ | std::views::values) {
        if (!line_value) {
            continue;
        }

        out << "\n\t";
        line_value->dump(out);

        const auto profile = find_profile(line_value->name)
                                 .value_or(make_line_profile(line_value->name));
        out << " profile(width=" << profile.visual_width
            << ", interaction=" << profile.interaction_width
            << ", length=" << profile.effective_length
            << ", damping=" << profile.damping << ")";
    }
}

} // namespace yodau::core
