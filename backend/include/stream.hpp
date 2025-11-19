#ifndef YODAU_BACKEND_STREAM_HPP
#define YODAU_BACKEND_STREAM_HPP
#include "geometry.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace yodau::backend {
enum stream_type { local, file, rtsp, http };

enum class stream_pipeline { manual, automatic, none };

class stream {
public:
    stream(
        std::string path, std::string name, std::string type = {},
        bool loop = true
    );
    static stream_type identify(const std::string& path);
    static std::string type_name(const stream_type type);
    static std::string pipeline_name(const stream_pipeline pipeline);

    void dump(std::ostream out, bool connections = false) const;

    void activate(stream_pipeline pipeline = stream_pipeline::automatic);
    stream_pipeline pipeline() const;
    void deactivate();

    void connect_line(line_ptr line);
    std::vector<std::string> line_names() const;

private:
    std::string name;
    std::string path;
    stream_type type;
    bool loop { true };
    stream_pipeline active { stream_pipeline::none };
    std::unordered_map<std::string, line_ptr> lines;
};
}
#endif // YODAU_BACKEND_STREAM_HPP