#ifndef YODAU_CORE_STREAM_DAEMON_RUNNER_HPP
#define YODAU_CORE_STREAM_DAEMON_RUNNER_HPP

#include "core/namespace_alias.hpp"
#include "streams/frame.hpp"
#include "streams/stream.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <unordered_map>

namespace yodau::core {

using stream_daemon_start_fn = std::function<void(
    const stream& s, std::function<void(frame&&)> on_frame,
    std::stop_token st
)>;
using stream_daemon_push_fn
    = std::function<void(const std::string& stream_name, frame&& f)>;

class stream_daemon_runner {
public:
    bool start(
        const std::string& name, std::shared_ptr<stream> stream_ptr,
        stream_daemon_start_fn daemon_fn, stream_daemon_push_fn push_fn
    );

    bool stop(const std::string& name);
    [[nodiscard]] bool is_running(const std::string& name) const;

private:
    static void run(
        std::string stream_name, std::shared_ptr<stream> stream_ptr,
        stream_daemon_start_fn daemon_fn, stream_daemon_push_fn push_fn,
        std::stop_token st
    );

    std::unordered_map<std::string, std::jthread> daemons_;
    mutable std::mutex mtx_;
};

} // namespace yodau::core

#endif // YODAU_CORE_STREAM_DAEMON_RUNNER_HPP
