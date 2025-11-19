#include "stream.hpp"

#include <unordered_map>

yodau::backend::stream::stream(
    std::string path, std::string name, std::string type, bool loop
)
    : name(std::move(name))
    , path(std::move(path))
    , loop(loop) {
    auto identified_type = identify(this->path);
    if (type.empty() || type == type_name(identified_type)) {
        type = identified_type;
    }
}

yodau::backend::stream_type yodau::backend::stream::identify(std::string) {
    return stream_type::local;
}

void yodau::backend::stream::dump(std::ostream out) const {
    out << "--name=" << name << " --path=" << path
        << " --type=" << type_name(type)
        << " --loop=" << (loop ? "true" : "false")
        << " --active=" << (active ? "true" : "false");
}

static std::string yodau::backend::stream::type_name(const stream_type type) {
    static const std::unordered_map<stream_type, std::string> type_names {
        { stream_type::local, "local" },
        { stream_type::file, "file" },
        { stream_type::rtsp, "rtsp" },
        { stream_type::http, "http" },
    };
    return type_names.at(type);
}

void yodau::backend::stream::activate() { active = true; }

void yodau::backend::stream::deactivate() { active = false; }

void yodau::backend::stream::connect_line(line_ptr line) {
    lines.push_back(std::move(line));
}
