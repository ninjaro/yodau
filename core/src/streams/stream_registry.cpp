#include "streams/stream_registry.hpp"

#include <ostream>
#include <ranges>
#include <utility>

namespace yodau::core {

stream& stream_registry::add(
    const std::string& path, const std::string& name, const std::string& type,
    const bool loop
) {
    std::string stream_name = name;
    while (stream_name.empty() || streams_.contains(stream_name)) {
        stream_name = "stream_" + std::to_string(stream_idx_++);
    }

    auto new_stream = std::make_shared<stream>(path, stream_name, type, loop);
    auto& ref = *new_stream;
    streams_.emplace(stream_name, std::move(new_stream));
    return ref;
}

bool stream_registry::add_detected(stream detected_stream) {
    const std::string name = detected_stream.get_name();
    if (streams_.contains(name)) {
        return false;
    }

    streams_.emplace(
        name, std::make_shared<stream>(std::move(detected_stream))
    );
    return true;
}

bool stream_registry::contains(const std::string& name) const {
    return streams_.contains(name);
}

std::shared_ptr<stream> stream_registry::find(const std::string& name) const {
    const auto it = streams_.find(name);
    return it == streams_.end() ? nullptr : it->second;
}

std::shared_ptr<const stream> stream_registry::find_const(
    const std::string& name
) const {
    const auto it = streams_.find(name);
    return it == streams_.end() ? nullptr : it->second;
}

std::vector<std::shared_ptr<stream>> stream_registry::snapshot() const {
    std::vector<std::shared_ptr<stream>> snap;
    snap.reserve(streams_.size());

    for (auto& stream_ptr : streams_ | std::views::values) {
        if (stream_ptr) {
            snap.push_back(stream_ptr);
        }
    }

    return snap;
}

std::vector<std::string> stream_registry::names() const {
    return streams_ | std::views::keys
        | std::ranges::to<std::vector<std::string>>();
}

size_t stream_registry::size() const { return streams_.size(); }

void stream_registry::dump(std::ostream& out, const bool connections) const {
    out << streams_.size() << " streams:";
    for (const auto& stream_ptr : streams_ | std::views::values) {
        if (!stream_ptr) {
            continue;
        }

        out << "\n\t";
        stream_ptr->dump(out, connections);
    }
}

} // namespace yodau::core
