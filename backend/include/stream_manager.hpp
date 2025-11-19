#ifndef YODAU_BACKEND_STREAM_MANAGER_HPP
#define YODAU_BACKEND_STREAM_MANAGER_HPP
#include "stream.hpp"
#include <functional>
#include <vector>

namespace yodau::backend {
class stream_manager {
public:
    using local_stream_detector_fn = std::function<std::vector<stream>()>;
    stream_manager();
    void dump(std::ostream out) const;
    void set_local_stream_detector(local_stream_detector_fn detector);
};
}

#endif // YODAU_BACKEND_STREAM_MANAGER_HPP