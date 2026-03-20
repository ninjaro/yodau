#ifndef YODAU_BACKEND_CLI_CLIENT_HPP
#define YODAU_BACKEND_CLI_CLIENT_HPP

#include "analysis/processing_runtime.hpp"
#include <cxxopts.hpp>

#include "streams/stream_manager.hpp"

namespace yodau::backend {
class cli_client {
public:
    explicit cli_client(backend::stream_manager& mgr);
    int run() const;

private:
    static std::vector<std::string> tokenize(const std::string& line);
    void dispatch_command(
        const std::string& cmd, const std::vector<std::string>& args
    ) const;
    static cxxopts::ParseResult parse_with_cxxopts(
        const std::string& cmd, const std::vector<std::string>& args,
        cxxopts::Options& options
    );
    void cmd_list_streams(const std::vector<std::string>& args) const;
    void cmd_add_stream(const std::vector<std::string>& args) const;
    void cmd_start_stream(const std::vector<std::string>& args) const;
    void cmd_stop_stream(const std::vector<std::string>& args) const;
    void cmd_list_lines(const std::vector<std::string>& args) const;
    void cmd_add_line(const std::vector<std::string>& args) const;
    void cmd_set_line(const std::vector<std::string>& args) const;
    void cmd_list_virtual_cameras(const std::vector<std::string>& args) const;
    void on_backend_events(const std::vector<event>& events) const;
    static void print_backend_event(const event& event_value);

    processing_runtime backend_runtime;
    backend::stream_manager& stream_mgr;
};
}

#endif // YODAU_BACKEND_CLI_CLIENT_HPP
