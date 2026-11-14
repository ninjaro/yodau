#include "cli/cli_client.hpp"
#include "analysis/default_processing_hooks.hpp"
#include "cli_client_internal.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>

yodau::core::cli_client::cli_client(yodau::core::stream_manager& mgr)
    : core_runtime(
          processing_runtime_options {
              .mode = render_mode::core_only,
              .enable_virtual_camera = true,
              .virtual_camera_device = {},
              .algorithm_id = default_processing_algorithm_id(),
          }
      )
    , stream_mgr(mgr) {
    core_runtime.attach(stream_mgr);
    stream_mgr.set_event_batch_sink(
        std::bind_front(&cli_client::on_core_events, this)
    );
}

yodau::core::cli_client::~cli_client() {
    stream_mgr.shutdown();
    core_runtime.detach();
}

int yodau::core::cli_client::run() {
    std::string line;
    while (true) {
        std::cout << "yodau> " << std::flush;
        if (!std::getline(std::cin, line)) {
            stream_mgr.shutdown();
            return std::cin.eof() ? 0 : 1;
        }
        auto tokens = tokenize(line);
        if (tokens.empty()) {
            continue;
        }
        const auto& cmd = tokens[0];
        std::vector args(tokens.begin() + 1, tokens.end());
        if (cmd == "quit" || cmd == "q" || cmd == "exit") {
            break;
        }
        dispatch_command(cmd, args);
    }
    stream_mgr.shutdown();
    return 0;
}

std::vector<std::string>
yodau::core::cli_client::tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream stream(line);
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

void yodau::core::cli_client::dispatch_command(
    const std::string& cmd, const std::vector<std::string>& args
) {
    static const std::unordered_map<
        std::string, void (cli_client::*)(const std::vector<std::string>& args)>
        command_map
        = { { "list-streams", &cli_client::cmd_list_streams },
            { "list-algorithms", &cli_client::cmd_list_algorithms },
            { "add-stream", &cli_client::cmd_add_stream },
            { "start-stream", &cli_client::cmd_start_stream },
            { "stop-stream", &cli_client::cmd_stop_stream },
            { "list-lines", &cli_client::cmd_list_lines },
            { "add-line", &cli_client::cmd_add_line },
            { "set-line", &cli_client::cmd_set_line },
            { "set-stream-algorithm", &cli_client::cmd_set_stream_algorithm },
            { "set-default-algorithm", &cli_client::cmd_set_default_algorithm },
            { "list-virtual-cameras", &cli_client::cmd_list_virtual_cameras },
            { "set-log-mode", &cli_client::cmd_set_log_mode },
            { "show-log", &cli_client::cmd_show_log },
            { "clear-log", &cli_client::cmd_clear_log } };
    const auto it = command_map.find(cmd);
    if (it == command_map.end()) {
        log_command(
            cli_log_severity::error, "cli_dispatch", "unknown command", {}, cmd
        );
        return;
    }
    try {
        const auto method = it->second;
        (this->*method)(args);
    } catch (const std::exception& e) {
        log_command(
            cli_log_severity::error, "cli_dispatch", "command execution failed",
            {}, cmd + ": " + e.what()
        );
    }
}

cxxopts::ParseResult yodau::core::cli_client::parse_args(
    const std::string& cmd, const std::vector<std::string>& args,
    cxxopts::Options& options
) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    argv.push_back(const_cast<char*>(cmd.data()));
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.data()));
    }
    const int argc = static_cast<int>(argv.size());
    char** argv_ptr = argv.data();
    return options.parse(argc, argv_ptr);
}
