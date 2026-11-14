#include "cli/cli_client.hpp"
#include "cli_client_internal.hpp"
#include "streams/virtual_camera.hpp"

#include <algorithm>
#include <iostream>

void yodau::core::cli_client::cmd_list_streams(
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
        const auto result = parse_args(cmd, args, options);
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
                + cli_detail::bool_text(show_connections)
        );
    } catch (const cxxopts::exceptions::exception& e) {
        log_command(
            cli_log_severity::error, "stream_inventory",
            "failed to parse list-streams", {}, e.what()
        );
        std::cout << options.help() << std::endl;
    }
}

void yodau::core::cli_client::cmd_add_stream(
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
        const auto result = parse_args(cmd, args, options);
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
                + " loop=" + cli_detail::bool_text(loop)
        );
    } catch (const cxxopts::exceptions::exception& e) {
        log_command(
            cli_log_severity::error, "stream_add", "failed to parse add-stream",
            {}, e.what()
        );
        std::cout << options.help() << std::endl;
    }
}

void yodau::core::cli_client::cmd_start_stream(
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
        const auto result = parse_args("start-stream", args, options);
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

void yodau::core::cli_client::cmd_stop_stream(
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
        const auto result = parse_args("stop-stream", args, options);
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
        if (auto* camera = core_runtime.preview_camera()) {
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

void yodau::core::cli_client::cmd_list_virtual_cameras(
    const std::vector<std::string>& args
) {
    const std::string cmd = "list-virtual-cameras";
    cxxopts::Options options(cmd, "List core virtual camera device bindings");
    options.allow_unrecognised_options();
    options.add_options()("h,help", "Print help");
    try {
        const auto result = parse_args(cmd, args, options);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return;
        }

        const auto* camera = core_runtime.preview_camera();
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
