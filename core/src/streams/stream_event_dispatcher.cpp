#include "streams/stream_event_dispatcher.hpp"

#include <utility>

namespace yodau::core {

void stream_event_sinks::dispatch(
    const std::vector<event>& events, const bool suppress_empty_batch
) const {
    if (batch) {
        if (!suppress_empty_batch || !events.empty()) {
            batch(events);
        }
        return;
    }

    if (!single) {
        return;
    }

    for (const auto& event_value : events) {
        single(event_value);
    }
}

void stream_event_dispatcher::set_event_sink(stream_event_sink_fn fn) {
    event_sink_ = std::move(fn);
}

void stream_event_dispatcher::set_event_batch_sink(
    stream_event_batch_sink_fn fn
) {
    event_batch_sink_ = std::move(fn);
}

stream_event_sinks stream_event_dispatcher::snapshot() const {
    return stream_event_sinks {
        .single = event_sink_,
        .batch = event_batch_sink_,
    };
}

} // namespace yodau::core
