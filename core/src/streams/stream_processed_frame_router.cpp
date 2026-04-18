#include "streams/stream_processed_frame_router.hpp"

#include <utility>

namespace yodau::core {

void stream_processed_frame_router::set_sink(stream_processed_frame_sink_fn fn) {
    sink_ = std::move(fn);
}

stream_processed_frame_sink_fn stream_processed_frame_router::snapshot() const {
    return sink_;
}

} // namespace yodau::core
