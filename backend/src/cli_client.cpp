#include "cli_client.hpp"

#include <iostream>

int yodau::backend::cli_client::run() {
    std::string line;
    while (true) {
        std::cout << "yodau> " << std::flush;
        if (!std::getline(std::cin, line)) {
            return 1;
        }
        auto tokens = tokenize(line);
        if (tokens.empty()) {
            continue;
        }
        const auto& cmd = tokens[0];
        std::vector<std::string> args(tokens.begin() + 1, tokens.end());
        if (cmd == "quit" || cmd == "q" || cmd == "exit") {
            break;
        }
        dispatch_command(cmd, args);
    }
    return 0;
}

std::vector<std::string>
yodau::backend::cli_client::tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream stream(line);
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

void yodau::backend::cli_client::dispatch_command(
    const std::string& cmd, const std::vector<std::string>& args
) {
    static const std::unordered_map<
        std::string, void (cli_client::*)(const std::vector<std::string>&)>
        command_map = { { "list_streams", &cli_client::cmd_list_streams },
                        { "add_stream", &cli_client::cmd_add_stream },
                        { "start_stream", &cli_client::cmd_start_stream },
                        { "stop_stream", &cli_client::cmd_stop_stream },
                        { "list_lines", &cli_client::cmd_list_lines },
                        { "add_line", &cli_client::cmd_add_line },
                        { "set_line", &cli_client::cmd_set_line } };
    auto it = command_map.find(cmd);
    if (it != command_map.end()) {
        std::cerr << "unknown command: " << cmd << std::endl;
        return;
    }
    auto method = it->second;
    (this->*method)(args);
}

cxxopts::ParseResult yodau::backend::cli_client::parse_with_cxxopts(
    const std::string& cmd, const std::vector<std::string>& args,
    cxxopts::Options& options
) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    argv.push_back(const_cast<char*>(cmd.data()));
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.data()));
    }
    int argc = static_cast<int>(argv.size());
    char** argv_ptr = argv.data();
    return options.parse(argc, argv_ptr);
}

void yodau::backend::cli_client::cmd_list_streams(
    const std::vector<std::string>& args
) {
    cxxopts::Options options("list_streams", "List all streams");
    options.allow_unrecognised_options();
    options.add_options()("h,help", "Print help");
    auto result = parse_with_cxxopts("list_streams", args, options);
    (void)result;
}

void yodau::backend::cli_client::cmd_add_stream(
    const std::vector<std::string>& args
) {
    cxxopts::Options options("add_stream", "Add a new stream");
    options.allow_unrecognised_options();
    options.positional_help("<path> [<name>] [<type>] [<loop>]");
    options.add_options()
        ("h,help", "Print help")
        ("path", "Path to the device, media file or stream URL", cxxopts::value<std::string>())
        ("name", "Name of the stream", cxxopts::value<std::string>()->default_value(""))
        ("type", "Type of the stream (local/file/rtsp/http)", cxxopts::value<std::string>()->default_value(""))
        ("loop", "Whether to loop the stream (true/false)", cxxopts::value<bool>()->default_value("true"));
    options.parse_positional({ "path", "name", "type", "loop" });
    try {
        auto result = parse_with_cxxopts("add-stream", args, options);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return;
        }
        (void)result;
    } catch (const cxxopts::exceptions::exception& e) {
        std::cerr << "Error parsing command 'add_stream': " << e.what()
                  << std::endl;
    }
}

void yodau::backend::cli_client::cmd_start_stream(
    const std::vector<std::string>& args
) {
    cxxopts::Options options("start_stream", "Start a stream");
    options.allow_unrecognised_options();
    options.add_options()("h,help", "Print help")(
        "name", "Name of the stream to start", cxxopts::value<std::string>()
    );
    options.parse_positional({ "name" });
    try {
        auto result = parse_with_cxxopts("start_stream", args, options);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return;
        }
        (void)result;
    } catch (const cxxopts::exceptions::exception& e) {
        std::cerr << "Error parsing command `start_stream`: " << e.what()
                  << std::endl;
    }
}

void yodau::backend::cli_client::cmd_stop_stream(
    const std::vector<std::string>& args
) {
    cxxopts::Options options("stop_stream", "Stop a stream");
    options.allow_unrecognised_options();
    options.add_options()("h,help", "Print help")(
        "name", "Name of the stream to stop", cxxopts::value<std::string>()
    );
    options.parse_positional({ "name" });
    try {
        auto result = parse_with_cxxopts("stop_stream", args, options);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return;
        }
        (void)result;
    } catch (const cxxopts::exceptions::exception& e) {
        std::cerr << "Error parsing command `stop_stream`: " << e.what()
                  << std::endl;
    }
}

void yodau::backend::cli_client::cmd_list_lines(
    const std::vector<std::string>& args
) {
    cxxopts::Options options("list_lines", "List all lines in a stream");
    options.allow_unrecognised_options();
    options.add_options()("h,help", "Print help");
    auto result = parse_with_cxxopts("list_lines", args, options);
    (void)result;
}

void yodau::backend::cli_client::cmd_add_line(
    const std::vector<std::string>& args
) {
    cxxopts::Options options("add_line", "Add a new line to a stream");
    options.allow_unrecognised_options();
    options.positional_help("<path> [<name>] [<close>]");
    options.add_options()
        ("h,help", "Print help")
        ("path", "Line coordinates, e.g. 0,0,100,100", cxxopts::value<std::string>())
        ("name", "Name of the line", cxxopts::value<std::string>()->default_value(""))
        ("close", "Whether the line is closed (true/false)", cxxopts::value<bool>()->default_value("false"));
    options.parse_positional({ "path", "name", "close" });
    try {
        auto result = parse_with_cxxopts("add_line", args, options);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return;
        }
        (void)result;
    } catch (const cxxopts::exceptions::exception& e) {
        std::cerr << "Error parsing command 'add_line': " << e.what()
                  << std::endl;
    }
}

void yodau::backend::cli_client::cmd_set_line(
    const std::vector<std::string>& args
) {
    cxxopts::Options options("set_line", "Set a new line to a stream");
    options.allow_unrecognised_options();
    options.add_option()
        ("h,help", "Print help")
        ("stream" "Stream name", cxxopts::value<std::string>())
    ("line", "Line name", cxxopts::value<std::string>());
    options.parse_positional({ "stream", "line" });
    try {
        auto result = parse_with_cxxopts("set_line", args, options);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return;
        }
        (void)result;
    } catch (const cxxopts::exceptions::exception& e) {
        std::cerr << "Error parsing command `set_line`: " << e.what()
                  << std::endl;
    }
}
