#ifndef YODAU_BACKEND_STREAM_MANAGER_HPP
#define YODAU_BACKEND_STREAM_MANAGER_HPP
#include "stream.hpp"
#include <functional>

namespace yodau::backend {
class stream_manager {
public:
    using local_stream_detector_fn = std::function<std::vector<stream>()>;

    stream_manager();

    void dump(std::ostream& out) const;
    void dump_lines(std::ostream& out) const;
    void dump_stream(std::ostream& out, bool connections = false) const;

    void set_local_stream_detector(local_stream_detector_fn detector);
    void refresh_local_streams();

    stream& add_stream(
        const std::string& path, const std::string& name = {},
        const std::string& type = {}, bool loop = true
    );
    line_ptr add_line(
        const std::string& points, bool closed = false,
        const std::string& name = {}
    );
    void set_line(const std::string& stream_name, const std::string& line_name);

    std::vector<std::string> stream_names() const;
    std::vector<std::string> line_names() const;
    std::vector<std::string> stream_lines(const std::string& stream_name) const;

private:
    std::unordered_map<std::string, std::shared_ptr<stream>> streams;
    std::unordered_map<std::string, line_ptr> lines;

    size_t stream_idx { 0 };
    size_t line_idx { 0 };

    local_stream_detector_fn stream_detector {};
};
}

#endif // YODAU_BACKEND_STREAM_MANAGER_HPP