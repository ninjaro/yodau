#ifndef YODAU_CORE_STREAM_PROCESSED_FRAME_ROUTER_HPP
#define YODAU_CORE_STREAM_PROCESSED_FRAME_ROUTER_HPP

#include "streams/event.hpp"
#include "streams/frame.hpp"
#include "streams/stream.hpp"

#include <functional>
#include <vector>

namespace yodau::core {

using stream_processed_frame_sink_fn = std::function<
    void(const stream& s, const frame& f, const std::vector<event>& events)>;

class stream_processed_frame_router {
public:
    void set_sink(stream_processed_frame_sink_fn fn);

    [[nodiscard]] stream_processed_frame_sink_fn snapshot() const;

private:
    stream_processed_frame_sink_fn sink_;
};

} // namespace yodau::core

#endif // YODAU_CORE_STREAM_PROCESSED_FRAME_ROUTER_HPP
