#ifndef YODAU_CORE_STREAM_DEMO_EVENT_RUNNER_HPP
#define YODAU_CORE_STREAM_DEMO_EVENT_RUNNER_HPP

#include "concurrency/stoppable_thread.hpp"
#include "core/namespace_alias.hpp"
#include "streams/event.hpp"
#include "streams/frame.hpp"
#include "streams/stream.hpp"
#include "streams/stream_event_dispatcher.hpp"
#include "streams/stream_processed_frame_router.hpp"

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace yodau::core {

using stream_demo_frame_processor_fn
    = std::function<std::vector<event>(const stream& s, const frame& f)>;
using stream_demo_snapshot_fn
    = std::function<std::vector<std::shared_ptr<stream>>()>;

struct stream_demo_hooks {
    stream_demo_frame_processor_fn frame_processor;
    stream_processed_frame_sink_fn processed_frame_sink;
    stream_event_sinks event_sinks;
};

using stream_demo_hooks_snapshot_fn = std::function<stream_demo_hooks()>;

class stream_demo_event_runner {
public:
    ~stream_demo_event_runner() { stop(); }

    void start(
        int interval_ms, stream_demo_snapshot_fn snapshot_streams,
        stream_demo_hooks_snapshot_fn snapshot_hooks
    ) {
        if (!snapshot_streams || !snapshot_hooks) {
            return;
        }

        {
            std::scoped_lock lock(mtx_);

            if (interval_ms > 0) {
                interval_ms_ = interval_ms;
            }

            if (enabled_) {
                return;
            }

            enabled_ = true;
        }

        stoppable_thread thread(
            [this, snapshot_streams = std::move(snapshot_streams),
             snapshot_hooks
             = std::move(snapshot_hooks)](const stop_token& stop_token) {
                run(stop_token, snapshot_streams, snapshot_hooks);
            }
        );

        {
            std::scoped_lock lock(mtx_);
            if (!enabled_) {
                thread.request_stop();
                return;
            }
            thread_ = std::move(thread);
        }
    }

    void stop() {
        stoppable_thread thread;

        {
            std::scoped_lock lock(mtx_);

            if (!enabled_) {
                return;
            }

            enabled_ = false;
            thread = std::move(thread_);
            thread_ = stoppable_thread();
        }

        if (thread.joinable()) {
            thread.request_stop();
            wake_.notify_all();
        }
    }

private:
    void
    run(const stop_token& stop_token,
        const stream_demo_snapshot_fn& snapshot_streams,
        const stream_demo_hooks_snapshot_fn& snapshot_hooks) noexcept {
        frame dummy;

        while (!stop_token.stop_requested()) {
            try {
                const auto streams = snapshot_streams();
                const auto hooks = snapshot_hooks();

                if (hooks.frame_processor) {
                    for (const auto& stream_ptr : streams) {
                        if (!stream_ptr) {
                            continue;
                        }

                        auto events = hooks.frame_processor(*stream_ptr, dummy);

                        if (hooks.processed_frame_sink) {
                            hooks.processed_frame_sink(
                                *stream_ptr, dummy, events
                            );
                        }

                        hooks.event_sinks.dispatch(events, true);
                    }
                }
            } catch (...) {
                // Never allow an injected processor or sink to terminate the
                // process from this background thread.
                std::scoped_lock lock(mtx_);
                enabled_ = false;
                return;
            }

            std::unique_lock lock(mtx_);
            wake_.wait_for(lock, std::chrono::milliseconds(interval_ms_), [&] {
                return stop_token.stop_requested();
            });
        }
    }

    mutable std::mutex mtx_;
    mutable std::condition_variable_any wake_;
    stoppable_thread thread_;
    int interval_ms_ { 700 };
    bool enabled_ { false };
};

} // namespace yodau::core

#endif // YODAU_CORE_STREAM_DEMO_EVENT_RUNNER_HPP
