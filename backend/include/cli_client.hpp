#ifndef YODAU_BACKEND_CLI_CLIENT_HPP
#define YODAU_BACKEND_CLI_CLIENT_HPP

#include <cxxopts.hpp>

namespace yodau::cli {
class cli_client {
public:
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
    void cmd_add_stream(const std::vector<std::string>& args);
    void cmd_start_stream(const std::vector<std::string>& args);
    void cmd_stop_stream(const std::vector<std::string>& args);
    void cmd_list_lines(const std::vector<std::string>& args);
    void cmd_add_line(const std::vector<std::string>& args);
    void cmd_set_line(const std::vector<std::string>& args);
};
}

#endif // YODAU_BACKEND_CLI_CLIENT_HPP