#include "cli/cli_client.hpp"

#include "analysis/default_processing_hooks.hpp"
#include "streams/virtual_camera.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>

namespace yodau::backend::cli_client_support {

std::string string_from_bool(const bool value) {
    return value ? "true" : "false";
}

tripwire_dir parse_tripwire_dir(const std::string& text) {
    if (text == "neg_to_pos") {
        return tripwire_dir::neg_to_pos;
    }
    if (text == "pos_to_neg") {
        return tripwire_dir::pos_to_neg;
    }
    if (text == "any") {
        return tripwire_dir::any;
    }
    throw std::runtime_error("unknown dir: " + text);
}

std::string normalized_algorithm_command(std::string text) {
    return processing_algorithm_registry::normalized_algorithm_id(text);
}

} // namespace yodau::backend::cli_client_support

yodau::backend::cli_client::cli_client(backend::stream_manager& mgr)
    : backend_runtime(
          processing_runtime_options {
              .mode = render_mode::backend_only,
              .enable_virtual_camera = true,
              .algorithm_id = default_processing_algorithm_id(),
          }
      )
    , stream_mgr(mgr) {
    backend_runtime.attach(stream_mgr);
    stream_mgr.set_event_batch_sink(
        std::bind_front(&cli_client::on_backend_events, this)
    );
}

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
        std::vector args(tokens.begin() + 1, tokens.end());
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
    const int argc = static_cast<int>(argv.size());
    char** argv_ptr = argv.data();
    return options.parse(argc, argv_ptr);
}

void yodau::backend::cli_client::cmd_list_streams(
    const std::vector<std::string>& args
) {
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
        log_command(
            cli_log_severity::debug, "stream_inventory", "listed streams", {},
            std::string("connections=")
                + cli_client_support::string_from_bool(show_connections)
        );
    } catch (const cxxopts::exceptions::exception& e) {
        log_command(
            cli_log_severity::error, "stream_inventory",
            "failed to parse list-streams", {}, e.what()
        );
        std::cout << options.help() << std::endl;
    }
}

void yodau::backend::cli_client::cmd_list_algorithms(
    const std::vector<std::string>& args
) {
    const std::string cmd = "list-algorithms";
    cxxopts::Options options(cmd, "List available backend algorithms");
    options.allow_unrecognised_options();
    options.add_options()("h,help", "Print help");

    try {
        const auto result = parse_with_cxxopts(cmd, args, options);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return;
        }

        const std::vector<std::string> algorithm_ids
            = backend_runtime.available_algorithm_ids();
        const std::string default_algorithm_id
            = backend_runtime.default_algorithm_id();
        const auto overrides = backend_runtime.stream_algorithm_overrides();
        const auto& registry = default_processing_algorithm_registry();

        std::cout << algorithm_ids.size()
                  << " algorithms (default=" << default_algorithm_id << "):"
                  << std::endl;
        for (const std::string& algorithm_id : algorithm_ids) {
            std::cout << "\t" << algorithm_id;
            if (const auto entry = registry.find(algorithm_id)) {
                std::cout << " (" << entry->display_name << ")";
            }
            if (algorithm_id == default_algorithm_id) {
                std::cout << " [default]";
            }
            std::cout << std::endl;
        }

        if (overrides.empty()) {
            std::cout << "0 stream overrides" << std::endl;
        } else {
            std::vector<std::pair<std::string, std::string>> override_rows(
                overrides.begin(), overrides.end()
            );
            std::sort(override_rows.begin(), override_rows.end());

            std::cout << override_rows.size() << " stream overrides:"
                      << std::endl;
            for (const auto& [stream_name, algorithm_id] : override_rows) {
                std::cout << "\t" << stream_name << " -> " << algorithm_id
                          << std::endl;
            }
        }

        log_command(
            cli_log_severity::debug, "algorithm_inventory",
            "listed algorithms", {},
            "count=" + std::to_string(algorithm_ids.size())
                + " default=" + default_algorithm_id
        );
    } catch (const cxxopts::exceptions::exception& e) {
        log_command(
            cli_log_severity::error, "algorithm_inventory",
            "failed to parse list-algorithms", {}, e.what()
        );
        std::cout << options.help() << std::endl;
    }
}

void yodau::backend::cli_client::cmd_add_stream(
    const std::vector<std::string>& args
) {
    const std::string cmd = "add-stream";
    cxxopts::Options options(cmd, "Add a new stream");
    options.allow_unrecognised_options();
    options.positional_help("<path> [<name>] [<type>] [<loop>]");
    options.add_options()
        ("h,help", "Print help")
        (
            "path", "Path to the device, media file or stream URL",
            cxxopts::value<std::string>()
        )
        (
            "name", "Name of the stream",
            cxxopts::value<std::string>()->default_value("")
        )
        (
            "type", "Type of the stream (local/file/rtsp/http)",
            cxxopts::value<std::string>()->default_value("")
        )
        (
            "loop", "Whether to loop the stream (true/false)",
            cxxopts::value<bool>()->default_value("true")
        );
    options.parse_positional({ "path", "name", "type", "loop" });
    try {
        const auto result = parse_with_cxxopts(cmd, args, options);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return;
        }
        if (!result.count("path")) {
            log_command(
                cli_log_severity::error, "stream_add",
                "path argument is required"
            );
            return;
        }
        const std::string path = result["path"].as<std::string>();
        const std::string name = result["name"].as<std::string>();
        const std::string type = result["type"].as<std::string>();
        const bool loop = result["loop"].as<bool>();
        const auto& stream = stream_mgr.add_stream(path, name, type, loop);
        stream.dump(std::cout, true);
        std::cout << std::endl;
        log_command(
            cli_log_severity::info, "stream_add", "stream added",
            stream.get_name(),
            "type=" + stream::type_name(stream.get_type()) + " path=" + path
                + " loop=" + cli_client_support::string_from_bool(loop)
        );
    } catch (const cxxopts::exceptions::exception& e) {
        log_command(
            cli_log_severity::error, "stream_add", "failed to parse add-stream",
            {}, e.what()
        );
        std::cout << options.help() << std::endl;
    }
}

void yodau::backend::cli_client::cmd_start_stream(
    const std::vector<std::string>& args
) {
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
            log_command(
                cli_log_severity::error, "stream_control",
                "stream name is required for start-stream"
            );
            return;
        }
        const std::string name = result["name"].as<std::string>();
        stream_mgr.start_stream(name);
        log_command(
            cli_log_severity::info, "stream_control", "stream started", name,
            "expects Linux virtual camera output or YODAU_VCAM_DEVICE"
        );
    } catch (const cxxopts::exceptions::exception& e) {
        log_command(
            cli_log_severity::error, "stream_control",
            "failed to parse start-stream", {}, e.what()
        );
        std::cout << options.help() << std::endl;
    }
}

void yodau::backend::cli_client::cmd_stop_stream(
    const std::vector<std::string>& args
) {
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
            log_command(
                cli_log_severity::error, "stream_control",
                "stream name is required for stop-stream"
            );
            return;
        }
        const std::string name = result["name"].as<std::string>();
        stream_mgr.stop_stream(name);
        if (auto* camera = backend_runtime.preview_camera()) {
            camera->release(name);
        }
        log_command(
            cli_log_severity::info, "stream_control", "stream stopped", name
        );
    } catch (const cxxopts::exceptions::exception& e) {
        log_command(
            cli_log_severity::error, "stream_control",
            "failed to parse stop-stream", {}, e.what()
        );
        std::cout << options.help() << std::endl;
    }
}

void yodau::backend::cli_client::cmd_list_lines(
    const std::vector<std::string>& args
) {
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
        log_command(cli_log_severity::debug, "line_inventory", "listed lines");
    } catch (const cxxopts::exceptions::exception& e) {
        log_command(
            cli_log_severity::error, "line_inventory",
            "failed to parse list-lines", {}, e.what()
        );
        std::cout << options.help() << std::endl;
    }
}

void yodau::backend::cli_client::cmd_add_line(
    const std::vector<std::string>& args
) {
    const std::string cmd = "add-line";
    cxxopts::Options options("add-line", "Add a new line to a stream");
    options.allow_unrecognised_options();
    options.positional_help("<path> [<name>] [<close>]");
    options.add_options()
        ("h,help", "Print help")
        (
            "path", "Line coordinates, e.g. 0,0;100,100",
            cxxopts::value<std::string>()
        )
        (
            "name", "Name of the line",
            cxxopts::value<std::string>()->default_value("")
        )
        (
            "close", "Whether the line is closed (true/false)",
            cxxopts::value<bool>()->default_value("false")
        )
        (
            "d,dir", "Tripwire direction (any/neg_to_pos/pos_to_neg)",
            cxxopts::value<std::string>()->default_value("any")
        )
        (
            "visual-width",
            "Backend visual width for the line profile",
            cxxopts::value<float>()
        )
        (
            "interaction-width",
            "Backend interaction width for the line profile",
            cxxopts::value<float>()
        )
        (
            "effective-length",
            "Backend effective length for the line profile",
            cxxopts::value<float>()
        )
        (
            "damping",
            "Backend damping value for the line profile",
            cxxopts::value<float>()
        );
    options.parse_positional({ "path", "name", "close" });
    try {
        const auto result = parse_with_cxxopts("add-line", args, options);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return;
        }
        if (!result.count("path")) {
            log_command(
                cli_log_severity::error, "line_edit",
                "line path is required for add-line"
            );
            return;
        }
        const std::string path = result["path"].as<std::string>();
        const std::string name = result["name"].as<std::string>();
        const bool close = result["close"].as<bool>();
        const std::string dir_str = result["dir"].as<std::string>();

        const auto& line = stream_mgr.add_line(path, close, name);

        if (result.count("visual-width") || result.count("interaction-width")
            || result.count("effective-length") || result.count("damping")) {
            auto profile
                = stream_mgr.find_line_profile(line->name).value_or(
                    make_line_profile(line->name)
                );

            if (result.count("visual-width")) {
                profile.visual_width = result["visual-width"].as<float>();
            }
            if (result.count("interaction-width")) {
                profile.interaction_width
                    = result["interaction-width"].as<float>();
            }
            if (result.count("effective-length")) {
                profile.effective_length
                    = result["effective-length"].as<float>();
            }
            if (result.count("damping")) {
                profile.damping = result["damping"].as<float>();
            }

            stream_mgr.set_line_profile(std::move(profile));
        }

        try {
            const auto dir = cli_client_support::parse_tripwire_dir(dir_str);
            stream_mgr.set_line_dir(line->name, dir);
        } catch (const std::exception& e) {
            log_command(
                cli_log_severity::warning, "line_edit",
                "invalid tripwire direction ignored", {}, e.what()
            );
        }

        line->dump(std::cout);
        std::cout << std::endl;
        log_command(
            cli_log_severity::info, "line_edit", "line added", {},
            "line=" + line->name
                + " closed=" + cli_client_support::string_from_bool(close)
        );

    } catch (const cxxopts::exceptions::exception& e) {
        log_command(
            cli_log_severity::error, "line_edit", "failed to parse add-line",
            {}, e.what()
        );
        std::cout << options.help() << std::endl;
    }
}

void yodau::backend::cli_client::cmd_set_line(
    const std::vector<std::string>& args
) {
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
            log_command(
                cli_log_severity::error, "line_edit",
                "stream and line arguments are required for set-line"
            );
            return;
        }
        const std::string stream_name = result["stream"].as<std::string>();
        const std::string line_name = result["line"].as<std::string>();
        const auto& stream = stream_mgr.set_line(stream_name, line_name);
        stream.dump(std::cout, true);
        std::cout << std::endl;
        log_command(
            cli_log_severity::info, "line_edit", "line attached to stream",
            stream_name, "line=" + line_name
        );
    } catch (const cxxopts::exceptions::exception& e) {
        log_command(
            cli_log_severity::error, "line_edit", "failed to parse set-line",
            {}, e.what()
        );
        std::cout << options.help() << std::endl;
    }
}

void yodau::backend::cli_client::cmd_set_stream_algorithm(
    const std::vector<std::string>& args
) {
    const std::string cmd = "set-stream-algorithm";
    cxxopts::Options options(cmd, "Set or clear a stream-specific algorithm");
    options.allow_unrecognised_options();
    options.add_options()
        ("h,help", "Print help")
        ("stream", "Stream name", cxxopts::value<std::string>())
        (
            "algorithm",
            "Algorithm id (or default/reset/clear to use the default algorithm)",
            cxxopts::value<std::string>()
        );
    options.parse_positional({ "stream", "algorithm" });

    try {
        const auto result = parse_with_cxxopts(cmd, args, options);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return;
        }
        if (!result.count("stream") || !result.count("algorithm")) {
            log_command(
                cli_log_severity::error, "algorithm_control",
                "stream and algorithm arguments are required for set-stream-algorithm"
            );
            return;
        }

        const std::string stream_name = result["stream"].as<std::string>();
        if (!stream_mgr.find_stream(stream_name)) {
            log_command(
                cli_log_severity::error, "algorithm_control",
                "stream not found for set-stream-algorithm", stream_name
            );
            return;
        }

        const std::string algorithm_text = result["algorithm"].as<std::string>();
        const std::string normalized_algorithm
            = cli_client_support::normalized_algorithm_command(algorithm_text);

        if (normalized_algorithm == "default" || normalized_algorithm == "reset"
            || normalized_algorithm == "clear") {
            backend_runtime.clear_stream_algorithm(stream_name);
            log_command(
                cli_log_severity::info, "algorithm_control",
                "stream algorithm reset to default", stream_name,
                "algorithm=" + backend_runtime.default_algorithm_id()
            );
            return;
        }

        if (!backend_runtime.set_stream_algorithm(stream_name, algorithm_text)) {
            log_command(
                cli_log_severity::error, "algorithm_control",
                "unknown stream algorithm", stream_name, algorithm_text
            );
            return;
        }

        log_command(
            cli_log_severity::info, "algorithm_control",
            "stream algorithm updated", stream_name,
            "algorithm=" + backend_runtime.algorithm_id_for_stream(stream_name)
        );
    } catch (const cxxopts::exceptions::exception& e) {
        log_command(
            cli_log_severity::error, "algorithm_control",
            "failed to parse set-stream-algorithm", {}, e.what()
        );
        std::cout << options.help() << std::endl;
    }
}

void yodau::backend::cli_client::cmd_set_default_algorithm(
    const std::vector<std::string>& args
) {
    const std::string cmd = "set-default-algorithm";
    cxxopts::Options options(cmd, "Set the default algorithm for new streams");
    options.allow_unrecognised_options();
    options.add_options()
        ("h,help", "Print help")
        ("algorithm", "Algorithm id", cxxopts::value<std::string>());
    options.parse_positional({ "algorithm" });

    try {
        const auto result = parse_with_cxxopts(cmd, args, options);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return;
        }
        if (!result.count("algorithm")) {
            log_command(
                cli_log_severity::error, "algorithm_control",
                "algorithm is required for set-default-algorithm"
            );
            return;
        }

        const std::string algorithm_text = result["algorithm"].as<std::string>();
        if (!backend_runtime.set_default_algorithm(algorithm_text)) {
            log_command(
                cli_log_severity::error, "algorithm_control",
                "unknown default algorithm", {}, algorithm_text
            );
            return;
        }

        log_command(
            cli_log_severity::info, "algorithm_control",
            "default algorithm updated", {},
            backend_runtime.default_algorithm_id()
        );
    } catch (const cxxopts::exceptions::exception& e) {
        log_command(
            cli_log_severity::error, "algorithm_control",
            "failed to parse set-default-algorithm", {}, e.what()
        );
        std::cout << options.help() << std::endl;
    }
}

void yodau::backend::cli_client::cmd_list_virtual_cameras(
    const std::vector<std::string>& args
) {
    const std::string cmd = "list-virtual-cameras";
    cxxopts::Options options(
        cmd, "List backend virtual camera device bindings"
    );
    options.allow_unrecognised_options();
    options.add_options()("h,help", "Print help");
    try {
        const auto result = parse_with_cxxopts(cmd, args, options);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return;
        }

        const auto* camera = backend_runtime.preview_camera();
        if (camera == nullptr) {
            std::cout << "0 virtual camera streams:" << std::endl;
            log_command(
                cli_log_severity::debug, "virtual_camera",
                "listed virtual camera bindings", {}, "count=0"
            );
            return;
        }

        camera->dump(std::cout);
        std::cout << std::endl;
        log_command(
            cli_log_severity::debug, "virtual_camera",
            "listed virtual camera bindings"
        );
    } catch (const cxxopts::exceptions::exception& e) {
        log_command(
            cli_log_severity::error, "virtual_camera",
            "failed to parse list-virtual-cameras", {}, e.what()
        );
        std::cout << options.help() << std::endl;
    }
}

void yodau::backend::cli_client::cmd_set_log_mode(
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
        const auto result = parse_with_cxxopts(cmd, args, options);
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

void yodau::backend::cli_client::cmd_show_log(
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
            "subsystem", "Optional subsystem filter",
            cxxopts::value<std::string>()->default_value("")
        );

    try {
        const auto result = parse_with_cxxopts(cmd, args, options);
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
        filter.subsystem = result["subsystem"].as<std::string>();

        const cli_log_mode mode = current_log_mode();
        const auto history = snapshot_log_history();

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

        const std::size_t limit_size = static_cast<std::size_t>(limit);
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

void yodau::backend::cli_client::cmd_clear_log(
    const std::vector<std::string>& args
) {
    const std::string cmd = "clear-log";
    cxxopts::Options options(cmd, "Clear CLI log history");
    options.allow_unrecognised_options();
    options.add_options()("h,help", "Print help");

    try {
        const auto result = parse_with_cxxopts(cmd, args, options);
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

void yodau::backend::cli_client::on_backend_events(
    const std::vector<event>& events
) {
    for (const auto& event_value : events) {
        append_log(make_event_log_entry(event_value));
    }
}

void yodau::backend::cli_client::append_log(cli_log_entry entry, bool echo) {
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

void yodau::backend::cli_client::log_command(
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

yodau::backend::cli_log_entry yodau::backend::cli_client::make_event_log_entry(
    const event& event_value
) const {
    cli_log_entry entry;
    entry.scope = cli_log_scope::event;
    entry.subsystem = "backend_event";
    entry.stream_name = event_value.stream_name;

    switch (event_value.kind) {
    case event_kind::motion:
        entry.severity = cli_log_severity::debug;
        entry.message = "motion detected";
        break;
    case event_kind::tripwire:
        entry.severity = cli_log_severity::info;
        entry.message = "tripwire triggered";
        break;
    case event_kind::roi:
        entry.severity = cli_log_severity::info;
        entry.message = "roi event";
        break;
    case event_kind::info:
    default:
        entry.severity = cli_log_severity::info;
        entry.message = "backend info event";
        break;
    }

    std::ostringstream detail;
    const std::string algorithm_id
        = backend_runtime.algorithm_id_for_stream(event_value.stream_name);
    if (!algorithm_id.empty()) {
        detail << "algorithm=" << algorithm_id;
    }
    if (!event_value.line_name.empty()) {
        if (!detail.str().empty()) {
            detail << ' ';
        }
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
        detail << "backend=" << event_value.message;
    }
    entry.detail = detail.str();
    return entry;
}

std::vector<yodau::backend::cli_log_entry>
yodau::backend::cli_client::snapshot_log_history() const {
    std::scoped_lock lock(log_mutex);
    return log_history;
}

yodau::backend::cli_log_mode
yodau::backend::cli_client::current_log_mode() const {
    std::scoped_lock lock(log_mutex);
    return active_log_mode;
}
