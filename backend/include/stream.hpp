#ifndef YODAU_BACKEND_STREAM_HPP
#define YODAU_BACKEND_STREAM_HPP
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace yodau::backend {

struct point {
    float x; // percentage [0.0; 100.0]
    float y; // percentage [0.0; 100.0]
};

struct line {
    std::string name;
    std::vector<point> points;
    bool closed { false };

    void dump(std::ostream& out) const;
    bool operator==(const line& other) const = default;
};

using line_ptr = std::shared_ptr<line const>;

enum stream_type { local, file, rtsp, http };

class stream {
public:
    stream(
        std::string path, std::string name, std::string type = "",
        bool loop = true
    );
    static stream_type identify(std::string path);
    void dump(std::ostream out) const;

    static std::string type_name(stream_type type);
    void activate();
    void deactivate();
    void connect_line(line_ptr line);

private:
    std::string name;
    std::string path;
    stream_type type;
    bool loop { true };
    bool active { false };
    std::vector<line_ptr> lines;
};
}
#endif // YODAU_BACKEND_STREAM_HPP