#ifndef YODAU_CORE_STREAM_REGISTRY_HPP
#define YODAU_CORE_STREAM_REGISTRY_HPP

#include "core/namespace_alias.hpp"
#include "streams/stream.hpp"

#include <cstddef>
#include <iosfwd>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace yodau::core {

class stream_registry {
public:
    stream&
    add(const std::string& path, const std::string& name = {},
        const std::string& type = {}, bool loop = true);

    bool add_detected(stream detected_stream);
    bool erase(const std::string& name);

    [[nodiscard]] bool contains(const std::string& name) const;

    [[nodiscard]] std::shared_ptr<stream> find(const std::string& name) const;
    [[nodiscard]] std::shared_ptr<const stream>
    find_const(const std::string& name) const;

    [[nodiscard]] std::vector<std::shared_ptr<stream>> snapshot() const;
    [[nodiscard]] std::vector<std::string> names() const;
    [[nodiscard]] size_t size() const;

    void dump(std::ostream& out, bool connections = false) const;

private:
    std::unordered_map<std::string, std::shared_ptr<stream>> streams_;
    size_t stream_idx_ { 0 };
};

} // namespace yodau::core

#endif // YODAU_CORE_STREAM_REGISTRY_HPP
