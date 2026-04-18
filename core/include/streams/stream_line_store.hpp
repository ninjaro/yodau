#ifndef YODAU_CORE_STREAM_LINE_STORE_HPP
#define YODAU_CORE_STREAM_LINE_STORE_HPP

#include "core/namespace_alias.hpp"
#include "geometry/geometry.hpp"
#include "streams/stream.hpp"

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace yodau::core {

struct stream_line_connection {
    line_ptr line;
    line_profile profile;
};

class stream_line_store {
public:
    line_ptr add(
        const std::string& points, bool closed = false,
        const std::string& name = {}
    );

    line_profile set_profile(line_profile profile_value);

    [[nodiscard]] std::optional<line_profile> find_profile(
        const std::string& line_name
    ) const;

    [[nodiscard]] stream_line_connection connection(
        const std::string& line_name
    ) const;

    [[nodiscard]] bool contains(const std::string& line_name) const;
    [[nodiscard]] std::vector<std::string> names() const;
    [[nodiscard]] size_t size() const;

    stream_line_connection set_direction(
        const std::string& line_name, tripwire_dir dir
    );

    void dump(std::ostream& out) const;

private:
    std::unordered_map<std::string, line_ptr> lines_;
    std::unordered_map<std::string, line_profile> line_profiles_;
    size_t line_idx_ { 0 };
};

} // namespace yodau::core

#endif // YODAU_CORE_STREAM_LINE_STORE_HPP
