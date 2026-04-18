#ifndef YODAU_CORE_STREAM_EVENT_DISPATCHER_HPP
#define YODAU_CORE_STREAM_EVENT_DISPATCHER_HPP

#include "core/namespace_alias.hpp"
#include "streams/event.hpp"

#include <functional>
#include <vector>

namespace yodau::core {

using stream_event_sink_fn = std::function<void(const event& e)>;
using stream_event_batch_sink_fn
    = std::function<void(const std::vector<event>& events)>;

struct stream_event_sinks {
    stream_event_sink_fn single;
    stream_event_batch_sink_fn batch;

    void dispatch(
        const std::vector<event>& events, bool suppress_empty_batch = false
    ) const;
};

class stream_event_dispatcher {
public:
    void set_event_sink(stream_event_sink_fn fn);
    void set_event_batch_sink(stream_event_batch_sink_fn fn);

    [[nodiscard]] stream_event_sinks snapshot() const;

private:
    stream_event_sink_fn event_sink_;
    stream_event_batch_sink_fn event_batch_sink_;
};

} // namespace yodau::core

#endif // YODAU_CORE_STREAM_EVENT_DISPATCHER_HPP
