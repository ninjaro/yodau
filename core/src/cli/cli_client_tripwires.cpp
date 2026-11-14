#include "cli/cli_client.hpp"
#include "cli_client_internal.hpp"

#include <algorithm>
#include <iostream>

void yodau::core::cli_client::cmd_list_lines(
    const std::vector<std::string>& args
) {
    const std::string cmd = "list-lines";
    cxxopts::Options options("list-lines", "List all lines in a stream");
    options.allow_unrecognised_options();
    options.add_options()("h,help", "Print help");
    try {
        const auto result = parse_args("list-lines", args, options);
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

void yodau::core::cli_client::cmd_add_line(
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
        const auto result = parse_args("add-line", args, options);
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
            auto profile = stream_mgr.find_line_profile(line->name)
                               .value_or(make_line_profile(line->name));

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
            const auto dir = cli_detail::tripwire_direction(dir_str);
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
            "line=" + line->name + " closed=" + cli_detail::bool_text(close)
        );

    } catch (const cxxopts::exceptions::exception& e) {
        log_command(
            cli_log_severity::error, "line_edit", "failed to parse add-line",
            {}, e.what()
        );
        std::cout << options.help() << std::endl;
    }
}

void yodau::core::cli_client::cmd_set_line(
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
        const auto result = parse_args("set-line", args, options);
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
