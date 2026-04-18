#include "streams/stream_daemon_runner.hpp"

#include <utility>

namespace yodau::core {

bool stream_daemon_runner::start(
    const std::string& name, std::shared_ptr<stream> stream_ptr,
    stream_daemon_start_fn daemon_fn, stream_daemon_push_fn push_fn
) {
    if (!stream_ptr || !daemon_fn || !push_fn) {
        return false;
    }

    std::scoped_lock lock(mtx_);
    if (daemons_.contains(name)) {
        return false;
    }

    std::jthread thread(
        [stream_name = name, stream_ptr = std::move(stream_ptr),
         daemon_fn = std::move(daemon_fn),
         push_fn = std::move(push_fn)](std::stop_token st) mutable {
            run(
                std::move(stream_name), std::move(stream_ptr),
                std::move(daemon_fn), std::move(push_fn), st
            );
        }
    );
    daemons_.emplace(name, std::move(thread));
    return true;
}

bool stream_daemon_runner::stop(const std::string& name) {
    std::jthread thread;

    {
        std::scoped_lock lock(mtx_);
        const auto it = daemons_.find(name);
        if (it == daemons_.end()) {
            return false;
        }

        thread = std::move(it->second);
        daemons_.erase(it);
    }

    thread.request_stop();
    return true;
}

bool stream_daemon_runner::is_running(const std::string& name) const {
    std::scoped_lock lock(mtx_);
    return daemons_.contains(name);
}

void stream_daemon_runner::run(
    std::string stream_name, std::shared_ptr<stream> stream_ptr,
    stream_daemon_start_fn daemon_fn, stream_daemon_push_fn push_fn,
    std::stop_token st
) {
    daemon_fn(
        *stream_ptr,
        [push_fn = std::move(push_fn), stream_name = std::move(stream_name)](
            frame&& f
        ) {
            push_fn(stream_name, std::move(f));
        },
        st
    );
}

} // namespace yodau::core
