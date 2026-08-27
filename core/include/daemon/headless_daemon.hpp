#ifndef YODAU_CORE_DAEMON_HEADLESS_DAEMON_HPP
#define YODAU_CORE_DAEMON_HEADLESS_DAEMON_HPP

#include "concurrency/stoppable_thread.hpp"
#include "core/namespace_alias.hpp"

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

namespace yodau::core {

struct headless_daemon_options {
    std::filesystem::path configuration_path;
    std::optional<std::string> source_override;
    std::optional<std::string> output_device_override;
    std::optional<std::string> stream_name_override;
};

// Returns local paths unchanged and removes every potentially sensitive URL
// component from remote capture sources.
[[nodiscard]] std::string
redact_capture_source_for_log(std::string_view source);

// Runs the core capture/processing/output pipeline in the calling thread. This
// API deliberately has no Qt dependency and never creates a GUI application.
int run_headless_daemon(
    const headless_daemon_options& options, const stop_token& stop_token,
    std::ostream& output, std::ostream& errors
);

} // namespace yodau::core

#endif // YODAU_CORE_DAEMON_HEADLESS_DAEMON_HPP
