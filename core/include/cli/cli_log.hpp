#ifndef YODAU_BACKEND_CLI_CLI_LOG_HPP
#define YODAU_BACKEND_CLI_CLI_LOG_HPP

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace yodau::backend {

enum class cli_log_scope { command, event };
enum class cli_log_severity { debug, info, warning, error };
enum class cli_log_mode { release, debug };

struct cli_log_entry {
    std::chrono::system_clock::time_point timestamp {};
    cli_log_scope scope { cli_log_scope::command };
    cli_log_severity severity { cli_log_severity::info };
    std::string subsystem;
    std::string stream_name;
    std::string message;
    std::string detail;
};

struct cli_log_filter {
    std::optional<cli_log_severity> severity;
    std::string stream_name;
    std::string subsystem;
};

inline std::string cli_log_scope_name(const cli_log_scope scope) {
    switch (scope) {
    case cli_log_scope::command:
        return "command";
    case cli_log_scope::event:
    default:
        return "event";
    }
}

inline std::string cli_log_severity_name(const cli_log_severity severity) {
    switch (severity) {
    case cli_log_severity::debug:
        return "debug";
    case cli_log_severity::info:
        return "info";
    case cli_log_severity::warning:
        return "warn";
    case cli_log_severity::error:
    default:
        return "error";
    }
}

inline std::string cli_log_mode_name(const cli_log_mode mode) {
    switch (mode) {
    case cli_log_mode::debug:
        return "debug";
    case cli_log_mode::release:
    default:
        return "release";
    }
}

inline std::optional<cli_log_severity>
cli_log_severity_from_string(std::string_view text) {
    std::string normalized(text);
    std::transform(
        normalized.begin(), normalized.end(), normalized.begin(),
        [](const unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        }
    );

    if (normalized == "debug") {
        return cli_log_severity::debug;
    }
    if (normalized == "info") {
        return cli_log_severity::info;
    }
    if (normalized == "warn" || normalized == "warning") {
        return cli_log_severity::warning;
    }
    if (normalized == "error") {
        return cli_log_severity::error;
    }

    return std::nullopt;
}

inline std::optional<cli_log_mode>
cli_log_mode_from_string(std::string_view text) {
    std::string normalized(text);
    std::transform(
        normalized.begin(), normalized.end(), normalized.begin(),
        [](const unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        }
    );

    if (normalized == "release") {
        return cli_log_mode::release;
    }
    if (normalized == "debug") {
        return cli_log_mode::debug;
    }

    return std::nullopt;
}

inline std::tm
cli_log_local_time(const std::chrono::system_clock::time_point timestamp) {
    const std::time_t raw_time
        = std::chrono::system_clock::to_time_t(timestamp);
    std::tm local_time {};

#if defined(_WIN32)
    localtime_s(&local_time, &raw_time);
#else
    localtime_r(&raw_time, &local_time);
#endif

    return local_time;
}

inline std::string
format_cli_timestamp(const std::chrono::system_clock::time_point timestamp) {
    const std::tm local_time = cli_log_local_time(timestamp);
    char buffer[16] {};
    std::strftime(buffer, sizeof(buffer), "%H:%M:%S", &local_time);
    return buffer;
}

inline std::string format_cli_timestamp_debug(
    const std::chrono::system_clock::time_point timestamp
) {
    const std::tm local_time = cli_log_local_time(timestamp);
    char buffer[16] {};
    std::strftime(buffer, sizeof(buffer), "%H:%M:%S", &local_time);

    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                            timestamp.time_since_epoch()
                        )
                            .count()
        % 1000;

    std::ostringstream out;
    out << buffer << '.' << std::setw(3) << std::setfill('0') << millis;
    return out.str();
}

inline bool
cli_log_entry_visible(const cli_log_mode mode, const cli_log_entry& entry) {
    if (mode == cli_log_mode::release
        && entry.severity == cli_log_severity::debug) {
        return false;
    }

    return true;
}

inline bool cli_log_entry_matches(
    const cli_log_mode mode, const cli_log_entry& entry,
    const cli_log_filter& filter
) {
    if (!cli_log_entry_visible(mode, entry)) {
        return false;
    }

    if (filter.severity.has_value() && entry.severity != *filter.severity) {
        return false;
    }

    if (!filter.stream_name.empty()
        && entry.stream_name != filter.stream_name) {
        return false;
    }

    if (!filter.subsystem.empty() && entry.subsystem != filter.subsystem) {
        return false;
    }

    return true;
}

inline std::string
format_cli_log_entry(const cli_log_mode mode, const cli_log_entry& entry) {
    std::ostringstream out;

    if (mode == cli_log_mode::debug) {
        out << '[' << format_cli_timestamp_debug(entry.timestamp) << "] "
            << cli_log_severity_name(entry.severity)
            << " scope=" << cli_log_scope_name(entry.scope);

        if (!entry.subsystem.empty()) {
            out << " subsystem=" << entry.subsystem;
        }
        if (!entry.stream_name.empty()) {
            out << " stream=" << entry.stream_name;
        }
        if (!entry.message.empty()) {
            out << ' ' << entry.message;
        }
        if (!entry.detail.empty()) {
            out << " detail=" << entry.detail;
        }

        return out.str();
    }

    out << '[' << format_cli_timestamp(entry.timestamp) << "] "
        << cli_log_severity_name(entry.severity);

    if (!entry.stream_name.empty()) {
        out << ' ' << entry.stream_name;
    }
    if (!entry.message.empty()) {
        out << ' ' << entry.message;
    }

    return out.str();
}

} // namespace yodau::backend

#endif // YODAU_BACKEND_CLI_CLI_LOG_HPP
