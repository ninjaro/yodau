#include "analysis/default_processing_hooks.hpp"
#include "cli/cli_client.hpp"
#include "cli_client_internal.hpp"

#include <algorithm>
#include <iostream>

void yodau::core::cli_client::cmd_list_algorithms(
    const std::vector<std::string>& args
) {
    const std::string cmd = "list-algorithms";
    cxxopts::Options options(cmd, "List available core algorithms");
    options.allow_unrecognised_options();
    options.add_options()("h,help", "Print help");

    try {
        const auto result = parse_args(cmd, args, options);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return;
        }

        const std::vector<std::string> algorithm_ids
            = processing_runtime::available_algorithm_ids();
        const std::string default_algorithm_id
            = core_runtime.default_algorithm_id();
        const auto overrides = core_runtime.stream_algorithm_overrides();
        const auto& registry = default_processing_algorithm_registry();

        std::cout << algorithm_ids.size()
                  << " algorithms (default=" << default_algorithm_id
                  << "):" << std::endl;
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

            std::cout << override_rows.size()
                      << " stream overrides:" << std::endl;
            for (const auto& [stream_name, algorithm_id] : override_rows) {
                std::cout << "\t" << stream_name << " -> " << algorithm_id
                          << std::endl;
            }
        }

        log_command(
            cli_log_severity::debug, "algorithm_inventory", "listed algorithms",
            {},
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

void yodau::core::cli_client::cmd_set_stream_algorithm(
    const std::vector<std::string>& args
) {
    const std::string cmd = "set-stream-algorithm";
    cxxopts::Options options(cmd, "Set or clear a stream-specific algorithm");
    options.allow_unrecognised_options();
    options
        .add_options()("h,help", "Print help")("stream", "Stream name", cxxopts::value<std::string>())(
            "algorithm",
            "Algorithm id (or default/reset/clear to use the default "
            "algorithm)",
            cxxopts::value<std::string>()
        );
    options.parse_positional({ "stream", "algorithm" });

    try {
        const auto result = parse_args(cmd, args, options);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return;
        }
        if (!result.count("stream") || !result.count("algorithm")) {
            log_command(
                cli_log_severity::error, "algorithm_control",
                "stream and algorithm arguments are required for "
                "set-stream-algorithm"
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

        const std::string algorithm_text
            = result["algorithm"].as<std::string>();
        const std::string normalized_algorithm
            = cli_detail::algorithm_id(algorithm_text);

        if (normalized_algorithm == "default" || normalized_algorithm == "reset"
            || normalized_algorithm == "clear") {
            core_runtime.clear_stream_algorithm(stream_name);
            log_command(
                cli_log_severity::info, "algorithm_control",
                "stream algorithm reset to default", stream_name,
                "algorithm=" + core_runtime.default_algorithm_id()
            );
            return;
        }

        if (!core_runtime.set_stream_algorithm(stream_name, algorithm_text)) {
            log_command(
                cli_log_severity::error, "algorithm_control",
                "unknown stream algorithm", stream_name, algorithm_text
            );
            return;
        }

        log_command(
            cli_log_severity::info, "algorithm_control",
            "stream algorithm updated", stream_name,
            "algorithm=" + core_runtime.algorithm_id_for_stream(stream_name)
        );
    } catch (const cxxopts::exceptions::exception& e) {
        log_command(
            cli_log_severity::error, "algorithm_control",
            "failed to parse set-stream-algorithm", {}, e.what()
        );
        std::cout << options.help() << std::endl;
    }
}

void yodau::core::cli_client::cmd_set_default_algorithm(
    const std::vector<std::string>& args
) {
    const std::string cmd = "set-default-algorithm";
    cxxopts::Options options(cmd, "Set the default algorithm for new streams");
    options.allow_unrecognised_options();
    options.add_options()("h,help", "Print help")(
        "algorithm", "Algorithm id", cxxopts::value<std::string>()
    );
    options.parse_positional({ "algorithm" });

    try {
        const auto result = parse_args(cmd, args, options);
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

        const std::string algorithm_text
            = result["algorithm"].as<std::string>();
        if (!core_runtime.set_default_algorithm(algorithm_text)) {
            log_command(
                cli_log_severity::error, "algorithm_control",
                "unknown default algorithm", {}, algorithm_text
            );
            return;
        }

        log_command(
            cli_log_severity::info, "algorithm_control",
            "default algorithm updated", {}, core_runtime.default_algorithm_id()
        );
    } catch (const cxxopts::exceptions::exception& e) {
        log_command(
            cli_log_severity::error, "algorithm_control",
            "failed to parse set-default-algorithm", {}, e.what()
        );
        std::cout << options.help() << std::endl;
    }
}
