#include "streams/stream_daemon_runner.hpp"

#include <exception>
#include <utility>

namespace yodau::core {

stream_daemon_runner::~stream_daemon_runner() { stop_all(); }

bool stream_daemon_runner::start(
    const std::string& name, std::shared_ptr<stream> stream_ptr,
    stream_daemon_start_fn daemon_fn, stream_daemon_push_fn push_fn,
    stream_daemon_completion_fn completion_fn
) {
    if (!stream_ptr || !daemon_fn || !push_fn) {
        return false;
    }

    while (true) {
        std::jthread completed_thread;
        {
            std::scoped_lock lock(mtx_);
            if (const auto it = daemons_.find(name); it != daemons_.end()) {
                if (is_active(snapshot(*it->second.state).state)) {
                    return false;
                }
                completed_thread = std::move(it->second.thread);
                daemons_.erase(it);
            } else {
                auto state = std::make_shared<daemon_state>();
                std::jthread thread([stream_name = name,
                                     stream_ptr = std::move(stream_ptr),
                                     daemon_fn = std::move(daemon_fn),
                                     push_fn = std::move(push_fn),
                                     completion_fn = std::move(completion_fn),
                                     state](const std::stop_token& st) mutable {
                    run(stream_name, stream_ptr, daemon_fn, std::move(push_fn),
                        completion_fn, state, st);
                });
                daemons_.emplace(
                    name, daemon_entry { std::move(thread), std::move(state) }
                );
                return true;
            }
        }

        // Join a completed run outside the runner mutex. Another starter may
        // win the slot meanwhile; the next loop iteration detects that.
        if (completed_thread.joinable()) {
            completed_thread.join();
        }
    }
}

bool stream_daemon_runner::stop(const std::string& name) {
    std::jthread thread;

    {
        std::scoped_lock lock(mtx_);
        const auto it = daemons_.find(name);
        if (it == daemons_.end()) {
            return false;
        }

        set_status(*it->second.state, stream_daemon_state::stopping);
        thread = std::move(it->second.thread);
        daemons_.erase(it);
    }

    thread.request_stop();
    return true;
}

void stream_daemon_runner::stop_all() {
    std::unordered_map<std::string, daemon_entry> daemons;

    {
        std::scoped_lock lock(mtx_);
        daemons.swap(daemons_);
    }

    for (auto& entry : daemons) {
        const bool active = is_active(snapshot(*entry.second.state).state);
        if (active) {
            set_status(*entry.second.state, stream_daemon_state::stopping);
        }
        if (active && entry.second.thread.joinable()) {
            entry.second.thread.request_stop();
        }
    }
}

bool stream_daemon_runner::is_running(const std::string& name) const {
    std::scoped_lock lock(mtx_);
    const auto it = daemons_.find(name);
    return it != daemons_.end() && is_active(snapshot(*it->second.state).state);
}

std::optional<stream_daemon_status>
stream_daemon_runner::status(const std::string& name) const {
    std::scoped_lock lock(mtx_);
    const auto it = daemons_.find(name);
    if (it == daemons_.end()) {
        return std::nullopt;
    }
    return snapshot(*it->second.state);
}

void stream_daemon_runner::run(
    const std::string& stream_name, const std::shared_ptr<stream>& stream_ptr,
    const stream_daemon_start_fn& daemon_fn, stream_daemon_push_fn push_fn,
    const stream_daemon_completion_fn& completion_fn,
    const std::shared_ptr<daemon_state>& state, const std::stop_token& st
) {
    stream_daemon_status final_status;
    try {
        set_status(*state, stream_daemon_state::running);
        daemon_fn(
            *stream_ptr,
            [push_fn = std::move(push_fn), stream_name](frame&& f) {
                push_fn(stream_name, std::move(f));
            },
            st
        );
        final_status.state = stream_daemon_state::completed;
    } catch (const std::exception& error) {
        final_status.state = stream_daemon_state::failed;
        final_status.error = error.what();
    } catch (...) {
        final_status.state = stream_daemon_state::failed;
        final_status.error = "unknown exception escaped stream daemon";
    }

    set_status(*state, final_status.state, final_status.error);
    if (completion_fn) {
        try {
            completion_fn(stream_name, final_status);
        } catch (...) {
            // Completion reporting must never escape a jthread entry point.
        }
    }
}

stream_daemon_status stream_daemon_runner::snapshot(const daemon_state& state) {
    std::scoped_lock lock(state.mtx);
    return state.status;
}

void stream_daemon_runner::set_status(
    daemon_state& state, const stream_daemon_state value, std::string error
) {
    std::scoped_lock lock(state.mtx);
    state.status.state = value;
    state.status.error = std::move(error);
}

bool stream_daemon_runner::is_active(const stream_daemon_state state) {
    return state == stream_daemon_state::starting
        || state == stream_daemon_state::running
        || state == stream_daemon_state::stopping;
}

} // namespace yodau::core
