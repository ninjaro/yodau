#ifndef YODAU_CORE_STREAM_DAEMON_RUNNER_HPP
#define YODAU_CORE_STREAM_DAEMON_RUNNER_HPP

#include "concurrency/stoppable_thread.hpp"
#include "streams/frame.hpp"
#include "streams/stream.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

namespace yodau::core {

using stream_daemon_start_fn = std::function<void(
    const stream& s, std::function<void(frame&&)> on_frame, stop_token st
)>;
using stream_daemon_push_fn
    = std::function<void(const std::string& stream_name, frame&& f)>;

enum class stream_daemon_state {
    starting,
    running,
    stopping,
    completed,
    failed,
};

struct stream_daemon_status {
    stream_daemon_state state { stream_daemon_state::completed };
    std::string error;
};

using stream_daemon_completion_fn = std::function<
    void(const std::string& stream_name, const stream_daemon_status& status)>;

class stream_daemon_runner {
public:
    ~stream_daemon_runner();

    bool start(
        const std::string& name, std::shared_ptr<stream> stream_ptr,
        stream_daemon_start_fn daemon_fn, stream_daemon_push_fn push_fn,
        stream_daemon_completion_fn completion_fn = {}
    );

    bool stop(const std::string& name);
    void stop_all();
    [[nodiscard]] bool is_running(const std::string& name) const;
    [[nodiscard]] std::optional<stream_daemon_status>
    status(const std::string& name) const;

private:
    struct daemon_state {
        mutable std::mutex mtx;
        stream_daemon_status status { stream_daemon_state::starting, {} };
    };

    struct daemon_entry {
        stoppable_thread thread;
        std::shared_ptr<daemon_state> state;
    };

    static void
    run(const std::string& stream_name,
        const std::shared_ptr<stream>& stream_ptr,
        const stream_daemon_start_fn& daemon_fn, stream_daemon_push_fn push_fn,
        const stream_daemon_completion_fn& completion_fn,
        const std::shared_ptr<daemon_state>& state, const stop_token& st);

    static stream_daemon_status snapshot(const daemon_state& state);
    static void set_status(
        daemon_state& state, stream_daemon_state value, std::string error = {}
    );
    static bool is_active(stream_daemon_state state);

    std::unordered_map<std::string, daemon_entry> daemons_;
    mutable std::mutex mtx_;
};

} // namespace yodau::core

#endif // YODAU_CORE_STREAM_DAEMON_RUNNER_HPP
