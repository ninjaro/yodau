#include "cli_client.hpp"

#include <iostream>

yodau::cli::cli_client::cli_client(backend::stream_manager& mgr)
    : stream_mgr(mgr) { }

int yodau::cli::cli_client::run() const {
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
        std::vector args(tokens.begin() + 1, tokens.end());
        if (cmd == "quit" || cmd == "q" || cmd == "exit") {
            break;
        }
        dispatch_command(cmd, args);
    }
    return 0;
}

std::vector<std::string>
yodau::cli::cli_client::tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream stream(line);
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

void yodau::cli::cli_client::dispatch_command(
    const std::string& cmd, const std::vector<std::string>& args
) const {
    static const std::unordered_map<
        std::string,
        void (cli_client::*)(const std::vector<std::string>& args) const>
        command_map = { { "list-streams", &cli_client::cmd_list_streams },
                        { "add-stream", &cli_client::cmd_add_stream },
                        { "start-stream", &cli_client::cmd_start_stream },
                        { "stop-stream", &cli_client::cmd_stop_stream },
                        { "list-lines", &cli_client::cmd_list_lines },
                        { "add-line", &cli_client::cmd_add_line },
                        { "set-line", &cli_client::cmd_set_line } };
    const auto it = command_map.find(cmd);
    if (it == command_map.end()) {
        std::cerr << "unknown command: " << cmd << std::endl;
        return;
    }
    try {
        const auto method = it->second;
        (this->*method)(args);
    } catch (const std::exception& e) {
        std::cerr << "error executing command '" << cmd << "': " << e.what()
                  << std::endl;
    }
}

cxxopts::ParseResult yodau::cli::cli_client::parse_with_cxxopts(
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

void yodau::cli::cli_client::cmd_list_streams(
    const std::vector<std::string>& args
) const {
    const std::string cmd = "list-streams";
    cxxopts::Options options(cmd, "List all streams");
    options.allow_unrecognised_options();
    options.add_options()("h,help", "Print help")(
        "c,connections", "Show connected lines",
        cxxopts::value<bool>()->default_value("false")
    );
    try {
        const auto result = parse_with_cxxopts(cmd, args, options);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return;
        }
        const bool show_connections = result["connections"].as<bool>();
        stream_mgr.dump_stream(std::cout, show_connections);
        std::cout << std::endl;
    } catch (const cxxopts::exceptions::exception& e) {
        std::cerr << "Error parsing command '" << cmd << "': " << e.what()
                  << std::endl;
        std::cout << options.help() << std::endl;
    }
}

void yodau::cli::cli_client::cmd_add_stream(
    const std::vector<std::string>& args
) const {
    const std::string cmd = "add-stream";
    cxxopts::Options options(cmd, "Add a new stream");
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
        const auto result = parse_with_cxxopts(cmd, args, options);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return;
        }
        if (!result.count("path")) {
            std::cerr << "Error: 'path' argument is required." << std::endl;
            return;
        }
        const std::string path = result["path"].as<std::string>();
        const std::string name = result["name"].as<std::string>();
        const std::string type = result["type"].as<std::string>();
        const bool loop = result["loop"].as<bool>();
        const auto& stream = stream_mgr.add_stream(path, name, type, loop);
        stream.dump(std::cout, true);
        std::cout << std::endl;
    } catch (const cxxopts::exceptions::exception& e) {
        std::cerr << "Error parsing command '" << cmd << "': " << e.what()
                  << std::endl;
        std::cout << options.help() << std::endl;
    }
}

void yodau::cli::cli_client::cmd_start_stream(
    const std::vector<std::string>& args
) const {
    const std::string cmd = "start-stream";
    cxxopts::Options options("start-stream", "Start a stream");
    options.allow_unrecognised_options();
    options.add_options()("h,help", "Print help")(
        "name", "Name of the stream to start", cxxopts::value<std::string>()
    );
    options.parse_positional({ "name" });
    try {
        const auto result = parse_with_cxxopts("start-stream", args, options);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return;
        }
        if (!result.count("name")) {
            std::cerr << "Error: 'name' argument is required." << std::endl;
            return;
        }
        const std::string name = result["name"].as<std::string>();
        // todo
    } catch (const cxxopts::exceptions::exception& e) {
        std::cerr << "Error parsing command '" << cmd << "': " << e.what()
                  << std::endl;
        std::cout << options.help() << std::endl;
    }
}

void yodau::cli::cli_client::cmd_stop_stream(
    const std::vector<std::string>& args
) const {
    const std::string cmd = "stop-stream";
    cxxopts::Options options("stop-stream", "Stop a stream");
    options.allow_unrecognised_options();
    options.add_options()("h,help", "Print help")(
        "name", "Name of the stream to stop", cxxopts::value<std::string>()
    );
    options.parse_positional({ "name" });
    try {
        const auto result = parse_with_cxxopts("stop-stream", args, options);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return;
        }
        if (!result.count("name")) {
            std::cerr << "Error: 'name' argument is required." << std::endl;
            return;
        }
        const std::string name = result["name"].as<std::string>();
        // todo
    } catch (const cxxopts::exceptions::exception& e) {
        std::cerr << "Error parsing command '" << cmd << "': " << e.what()
                  << std::endl;
        std::cout << options.help() << std::endl;
    }
}

void yodau::cli::cli_client::cmd_list_lines(
    const std::vector<std::string>& args
) const {
    const std::string cmd = "list-lines";
    cxxopts::Options options("list-lines", "List all lines in a stream");
    options.allow_unrecognised_options();
    options.add_options()("h,help", "Print help");
    try {
        const auto result = parse_with_cxxopts("list-lines", args, options);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return;
        }
        stream_mgr.dump_lines(std::cout);
        std::cout << std::endl;
    } catch (const cxxopts::exceptions::exception& e) {
        std::cerr << "Error parsing command '" << cmd << "': " << e.what()
                  << std::endl;
        std::cout << options.help() << std::endl;
    }
}

void yodau::cli::cli_client::cmd_add_line(
    const std::vector<std::string>& args
) const {
    const std::string cmd = "add-line";
    cxxopts::Options options("add-line", "Add a new line to a stream");
    options.allow_unrecognised_options();
    options.positional_help("<path> [<name>] [<close>]");
    options.add_options()
        ("h,help", "Print help")
        ("path", "Line coordinates, e.g. 0,0,100,100", cxxopts::value<std::string>())
        ("name", "Name of the line", cxxopts::value<std::string>()->default_value(""))
        ("close", "Whether the line is closed (true/false)", cxxopts::value<bool>()->default_value("false"));
    options.parse_positional({ "path", "name", "close" });
    try {
        const auto result = parse_with_cxxopts("add-line", args, options);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return;
        }
        if (!result.count("path")) {
            std::cerr << "Error: 'path' argument is required." << std::endl;
            return;
        }
        const std::string path = result["path"].as<std::string>();
        const std::string name = result["name"].as<std::string>();
        const bool close = result["close"].as<bool>();
        const auto& line = stream_mgr.add_line(path, close, name);
        line->dump(std::cout);
        std::cout << std::endl;
    } catch (const cxxopts::exceptions::exception& e) {
        std::cerr << "Error parsing command '" << cmd << "': " << e.what()
                  << std::endl;
        std::cout << options.help() << std::endl;
    }
}

void yodau::cli::cli_client::cmd_set_line(
    const std::vector<std::string>& args
) const {
    const std::string cmd = "set-line";
    cxxopts::Options options("set-line", "Set a new line to a stream");
    options.allow_unrecognised_options();
    options
        .add_options()("h,help", "Print help")("stream", "Stream name", cxxopts::value<std::string>())(
            "line", "Line name", cxxopts::value<std::string>()
        );
    options.parse_positional({ "stream", "line" });
    try {
        const auto result = parse_with_cxxopts("set-line", args, options);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return;
        }
        if (!result.count("stream") || !result.count("line")) {
            std::cerr << "Error: 'stream' and 'line' arguments are required."
                      << std::endl;
            return;
        }
        const std::string stream_name = result["stream"].as<std::string>();
        const std::string line_name = result["line"].as<std::string>();
        const auto& stream = stream_mgr.set_line(stream_name, line_name);
        stream.dump(std::cout, true);
        std::cout << std::endl;
    } catch (const cxxopts::exceptions::exception& e) {
        std::cerr << "Error parsing command '" << cmd << "': " << e.what()
                  << std::endl;
        std::cout << options.help() << std::endl;
    }
}
