#include "cli/cli_client.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>

void yodau::core::cli_client::cmd_set_log_mode(
    const std::vector<std::string>& args
) {
    const std::string cmd = "set-log-mode";
    cxxopts::Options options(cmd, "Set CLI log output mode");
    options.allow_unrecognised_options();
    options.add_options()("h,help", "Print help")(
        "mode", "Log mode (release/debug)", cxxopts::value<std::string>()
    );
    options.parse_positional({ "mode" });
    try {
        const auto result = parse_args(cmd, args, options);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return;
        }
        if (!result.count("mode")) {
            log_command(
                cli_log_severity::error, "cli_log",
                "log mode is required for set-log-mode"
            );
            return;
        }

        const auto mode
            = cli_log_mode_from_string(result["mode"].as<std::string>());
        if (!mode.has_value()) {
            log_command(
                cli_log_severity::error, "cli_log", "unknown log mode", {},
                result["mode"].as<std::string>()
            );
            return;
        }

        {
            std::scoped_lock lock(log_mutex);
            active_log_mode = *mode;
        }

        log_command(
            cli_log_severity::info, "cli_log", "log mode updated", {},
            cli_log_mode_name(*mode)
        );
    } catch (const cxxopts::exceptions::exception& e) {
        log_command(
            cli_log_severity::error, "cli_log", "failed to parse set-log-mode",
            {}, e.what()
        );
        std::cout << options.help() << std::endl;
    }
}

void yodau::core::cli_client::cmd_show_log(
    const std::vector<std::string>& args
) {
    const std::string cmd = "show-log";
    cxxopts::Options options(cmd, "Show CLI log history");
    options.allow_unrecognised_options();
    options.add_options()
        ("h,help", "Print help")
        (
            "limit", "Maximum number of matching log entries to print",
            cxxopts::value<int>()->default_value("50")
        )
        (
            "severity", "Optional severity filter (debug/info/warning/error)",
            cxxopts::value<std::string>()->default_value("")
        )
        (
            "stream", "Optional stream-name filter",
            cxxopts::value<std::string>()->default_value("")
        )
        (
            "line", "Optional line or region-name filter",
            cxxopts::value<std::string>()->default_value("")
        )
        (
            "algorithm", "Optional algorithm-id filter",
            cxxopts::value<std::string>()->default_value("")
        )
        (
            "event", "Optional event-type filter",
            cxxopts::value<std::string>()->default_value("")
        )
        (
            "subsystem", "Optional subsystem filter",
            cxxopts::value<std::string>()->default_value("")
        );

    try {
        const auto result = parse_args(cmd, args, options);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return;
        }

        const int limit = result["limit"].as<int>();
        if (limit <= 0) {
            log_command(
                cli_log_severity::error, "cli_log",
                "show-log limit must be greater than zero"
            );
            return;
        }

        cli_log_filter filter;
        const std::string severity_text = result["severity"].as<std::string>();
        if (!severity_text.empty()) {
            const auto severity = cli_log_severity_from_string(severity_text);
            if (!severity.has_value()) {
                log_command(
                    cli_log_severity::error, "cli_log",
                    "unknown severity filter", {}, severity_text
                );
                return;
            }
            filter.severity = *severity;
        }
        filter.stream_name = result["stream"].as<std::string>();
        filter.line_name = result["line"].as<std::string>();
        filter.algorithm_id = result["algorithm"].as<std::string>();
        filter.event_type = result["event"].as<std::string>();
        filter.subsystem = result["subsystem"].as<std::string>();

        const cli_log_mode mode = log_mode();
        const auto history = log_snapshot();

        std::vector<cli_log_entry> filtered_entries;
        filtered_entries.reserve(history.size());
        for (const cli_log_entry& entry : history) {
            if (cli_log_entry_matches(mode, entry, filter)) {
                filtered_entries.push_back(entry);
            }
        }

        const std::size_t entry_count = filtered_entries.size();
        std::cout << entry_count << " matching log entries"
                  << " (mode=" << cli_log_mode_name(mode) << ')' << std::endl;

        const auto limit_size = static_cast<std::size_t>(limit);
        const std::size_t start_index
            = entry_count > limit_size ? entry_count - limit_size : 0u;

        for (std::size_t index = start_index; index < entry_count; ++index) {
            std::cout << format_cli_log_entry(mode, filtered_entries[index])
                      << std::endl;
        }
    } catch (const cxxopts::exceptions::exception& e) {
        log_command(
            cli_log_severity::error, "cli_log", "failed to parse show-log", {},
            e.what()
        );
        std::cout << options.help() << std::endl;
    }
}

void yodau::core::cli_client::cmd_clear_log(
    const std::vector<std::string>& args
) {
    const std::string cmd = "clear-log";
    cxxopts::Options options(cmd, "Clear CLI log history");
    options.allow_unrecognised_options();
    options.add_options()("h,help", "Print help");

    try {
        const auto result = parse_args(cmd, args, options);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return;
        }

        {
            std::scoped_lock lock(log_mutex);
            log_history.clear();
        }

        std::cout << "log history cleared" << std::endl;
    } catch (const cxxopts::exceptions::exception& e) {
        log_command(
            cli_log_severity::error, "cli_log", "failed to parse clear-log", {},
            e.what()
        );
        std::cout << options.help() << std::endl;
    }
}

void yodau::core::cli_client::on_core_events(const std::vector<event>& events) {
    for (const auto& event_value : events) {
        append_log(event_entry(event_value));
    }
}

void yodau::core::cli_client::append_log(cli_log_entry entry, bool echo) {
    if (entry.timestamp.time_since_epoch().count() == 0) {
        entry.timestamp = std::chrono::system_clock::now();
    }

    std::string formatted_line;
    {
        std::scoped_lock lock(log_mutex);

        log_history.push_back(entry);
        if (log_history.size() > max_log_entries) {
            const auto remove_count = log_history.size() - max_log_entries;
            log_history.erase(
                log_history.begin(),
                log_history.begin() + static_cast<std::ptrdiff_t>(remove_count)
            );
        }

        if (echo && cli_log_entry_visible(active_log_mode, entry)) {
            formatted_line = format_cli_log_entry(active_log_mode, entry);
        }
    }

    if (!formatted_line.empty()) {
        std::cout << formatted_line << std::endl;
    }
}

void yodau::core::cli_client::log_command(
    const cli_log_severity severity, const std::string& subsystem,
    const std::string& message, const std::string& stream_name,
    const std::string& detail
) {
    cli_log_entry entry;
    entry.scope = cli_log_scope::command;
    entry.severity = severity;
    entry.subsystem = subsystem;
    entry.stream_name = stream_name;
    entry.message = message;
    entry.detail = detail;
    append_log(std::move(entry));
}

yodau::core::cli_log_entry
yodau::core::cli_client::event_entry(const event& event_value) const {
    cli_log_entry entry;
    entry.scope = cli_log_scope::event;
    entry.subsystem = "core_event";
    entry.stream_name = event_value.stream_name;

    switch (event_value.kind) {
    case event_kind::motion:
        entry.severity = cli_log_severity::debug;
        entry.event_type = "motion";
        entry.message = "motion detected";
        break;
    case event_kind::tripwire:
        entry.severity = cli_log_severity::info;
        entry.event_type = "tripwire";
        entry.message = "tripwire triggered";
        break;
    case event_kind::roi:
        entry.severity = cli_log_severity::info;
        entry.event_type = "roi";
        entry.message = "roi event";
        break;
    case event_kind::info:
    default:
        entry.severity = cli_log_severity::info;
        entry.event_type = "info";
        entry.message = "core info event";
        break;
    }

    const std::string algorithm_id
        = core_runtime.algorithm_id_for_stream(event_value.stream_name);
    entry.algorithm_id = algorithm_id;
    entry.line_name = event_value.line_name;

    std::ostringstream detail;
    if (!event_value.line_name.empty()) {
        detail << "line=" << event_value.line_name;
    }
    if (event_value.pos_pct.has_value()) {
        if (!detail.str().empty()) {
            detail << ' ';
        }
        detail << "pos=(" << event_value.pos_pct->x << ','
               << event_value.pos_pct->y << ')';
    }
    if (!event_value.message.empty()) {
        if (!detail.str().empty()) {
            detail << ' ';
        }
        detail << "core=" << event_value.message;
    }
    entry.detail = detail.str();
    return entry;
}

std::vector<yodau::core::cli_log_entry>
yodau::core::cli_client::log_snapshot() const {
    std::scoped_lock lock(log_mutex);
    return log_history;
}

yodau::core::cli_log_mode yodau::core::cli_client::log_mode() const {
    std::scoped_lock lock(log_mutex);
    return active_log_mode;
}
