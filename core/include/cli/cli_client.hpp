#ifndef YODAU_BACKEND_CLI_CLIENT_HPP
#define YODAU_BACKEND_CLI_CLIENT_HPP

#include "analysis/processing_runtime.hpp"
#include "cli/cli_log.hpp"

#include <cxxopts.hpp>
#include <mutex>

#include "streams/stream_manager.hpp"

namespace yodau::backend {
class cli_client {
public:
    explicit cli_client(backend::stream_manager& mgr);
    int run();

private:
    static std::vector<std::string> tokenize(const std::string& line);
    void dispatch_command(
        const std::string& cmd, const std::vector<std::string>& args
    );
    static cxxopts::ParseResult parse_with_cxxopts(
        const std::string& cmd, const std::vector<std::string>& args,
        cxxopts::Options& options
    );
    void cmd_list_streams(const std::vector<std::string>& args);
    void cmd_list_algorithms(const std::vector<std::string>& args);
    void cmd_add_stream(const std::vector<std::string>& args);
    void cmd_start_stream(const std::vector<std::string>& args);
    void cmd_stop_stream(const std::vector<std::string>& args);
    void cmd_list_lines(const std::vector<std::string>& args);
    void cmd_add_line(const std::vector<std::string>& args);
    void cmd_set_line(const std::vector<std::string>& args);
    void cmd_set_stream_algorithm(const std::vector<std::string>& args);
    void cmd_set_default_algorithm(const std::vector<std::string>& args);
    void cmd_list_virtual_cameras(const std::vector<std::string>& args);
    void cmd_set_log_mode(const std::vector<std::string>& args);
    void cmd_show_log(const std::vector<std::string>& args);
    void cmd_clear_log(const std::vector<std::string>& args);
    void on_backend_events(const std::vector<event>& events);
    void append_log(cli_log_entry entry, bool echo = true);
    void log_command(
        cli_log_severity severity, const std::string& subsystem,
        const std::string& message, const std::string& stream_name = {},
        const std::string& detail = {}
    );
    cli_log_entry make_event_log_entry(const event& event_value) const;
    std::vector<cli_log_entry> snapshot_log_history() const;
    cli_log_mode current_log_mode() const;

    processing_runtime backend_runtime;
    backend::stream_manager& stream_mgr;
    mutable std::mutex log_mutex;
    std::vector<cli_log_entry> log_history;
    cli_log_mode active_log_mode { cli_log_mode::release };
    std::size_t max_log_entries { 512 };
};
} // namespace yodau::backend

#endif // YODAU_BACKEND_CLI_CLIENT_HPP
