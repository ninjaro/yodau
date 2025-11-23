#ifndef YODAU_BACKEND_STREAM_HPP
#define YODAU_BACKEND_STREAM_HPP
#include "geometry.hpp"
#include <mutex>
#include <unordered_map>

namespace yodau::backend {
enum stream_type { local, file, rtsp, http };

enum class stream_pipeline { manual, automatic, none };

class stream {
public:
    stream(
        std::string path, std::string name, const std::string& type_str = {},
        bool loop = true
    );
    stream(const stream&) = delete;
    stream& operator=(const stream&) = delete;
    stream(stream&& other) noexcept;
    stream& operator=(stream&& other) noexcept;

    static stream_type identify(const std::string& path);
    static std::string type_name(const stream_type type);
    static std::string pipeline_name(const stream_pipeline pipeline);

    std::string get_name() const;
    std::string get_path() const;
    stream_type get_type() const;
    bool is_looping() const;

    void dump(std::ostream& out, bool connections = false) const;

    void activate(stream_pipeline pipeline = stream_pipeline::automatic);
    stream_pipeline pipeline() const;
    void deactivate();

    void connect_line(line_ptr line);
    std::vector<std::string> line_names() const;
    std::vector<line_ptr> lines_snapshot() const;

private:
    std::string name;
    std::string path;
    stream_type type;
    bool loop { true };
    stream_pipeline active { stream_pipeline::none };
    std::unordered_map<std::string, line_ptr> lines;
    mutable std::mutex lines_mtx;
};
}
#endif // YODAU_BACKEND_STREAM_HPP