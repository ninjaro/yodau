#include "daemon/headless_daemon.hpp"

#include <cxxopts.hpp>

#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#if !defined(NDEBUG) && defined(__linux__) && !defined(__ANDROID__)
#include "monitor/client.hpp"
#endif

#ifndef YODAU_OPENCV
#error "yodau-daemon requires OpenCV capture and processing support"
#endif

namespace {

volatile std::sig_atomic_t stop_signal_received = 0;

extern "C" void handle_stop_signal(int) { stop_signal_received = 1; }

} // namespace

int main(int argc, char** argv) {
    cxxopts::Options options(
        "yodau-daemon",
        "Headless camera processor using line configurations exported by yodau"
    );
    options.add_options()("h,help", "Print help");
    options.add_options()(
        "c,config", "Line configuration JSON file",
        cxxopts::value<std::string>()
    );
    options.add_options()(
        "s,source", "Override the configured source camera/device",
        cxxopts::value<std::string>()
    );
    options.add_options()(
        "o,output", "Override the configured virtual camera device",
        cxxopts::value<std::string>()
    );
    options.add_options()(
        "n,name", "Override the configured stream name",
        cxxopts::value<std::string>()
    );

    try {
        const auto parsed = options.parse(argc, argv);
        if (parsed.count("help")) {
            std::cout << options.help() << '\n';
            return 0;
        }
        if (!parsed.count("config")) {
            std::cerr << options.help() << '\n';
            return 2;
        }

#if !defined(NDEBUG) && defined(__linux__) && !defined(__ANDROID__)
        auto& watchdog = monitor::client::process();
        const bool watchdog_started = watchdog.start("yodau-daemon");
        if (watchdog_started) {
            watchdog.set_channel_active(monitor::channel::main, true);
            watchdog.breadcrumb(monitor::event::process_started);
        }
#endif

        yodau::core::headless_daemon_options daemon_options;
        daemon_options.configuration_path = parsed["config"].as<std::string>();
        if (parsed.count("source")) {
            daemon_options.source_override = parsed["source"].as<std::string>();
        }
        if (parsed.count("output")) {
            daemon_options.output_device_override
                = parsed["output"].as<std::string>();
        }
        if (parsed.count("name")) {
            daemon_options.stream_name_override
                = parsed["name"].as<std::string>();
        }

        std::signal(SIGINT, handle_stop_signal);
        std::signal(SIGTERM, handle_stop_signal);

        yodau::core::stop_source stop_source;
        yodau::core::stoppable_thread signal_watcher(
            [&stop_source](const yodau::core::stop_token& watcher_stop) {
                while (!watcher_stop.stop_requested()
                       && stop_signal_received == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                if (stop_signal_received != 0) {
                    stop_source.request_stop();
                }
            }
        );

        const int result = yodau::core::run_headless_daemon(
            daemon_options, stop_source.get_token(), std::cout, std::cerr
        );
        signal_watcher.request_stop();
#if !defined(NDEBUG) && defined(__linux__) && !defined(__ANDROID__)
        if (watchdog_started) {
            watchdog.breadcrumb(monitor::event::process_stopping);
            watchdog.set_channel_active(monitor::channel::main, false);
            watchdog.stop();
        }
#endif
        return result;
    } catch (const cxxopts::exceptions::exception& error) {
        std::cerr << "invalid arguments: " << error.what() << '\n'
                  << options.help() << '\n';
        return 2;
    }
}
