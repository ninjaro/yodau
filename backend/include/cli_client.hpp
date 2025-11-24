#ifndef YODAU_BACKEND_CLI_CLIENT_HPP
#define YODAU_BACKEND_CLI_CLIENT_HPP

#include <cxxopts.hpp>

#include "stream_manager.hpp"

#include <string>
#include <vector>

namespace yodau::backend {

/**
 * @brief Simple interactive CLI (REPL) for controlling a @ref stream_manager.
 *
 * The cli_client provides a text-based command loop that allows the user to:
 * - list/add/start/stop streams,
 * - list/add lines,
 * - connect lines to streams,
 * using cxxopts-style option parsing.
 *
 * The client does not own the manager; it holds a reference and issues
 * synchronous calls to it.
 *
 * @note When compiled with YODAU_OPENCV, the constructor installs OpenCV-based
 *       daemon start and motion frame processor hooks (see implementation).
 */
class cli_client {
public:
    /**
     * @brief Construct a CLI client operating on an existing manager.
     *
     * The manager reference must remain valid for the lifetime of this client.
     *
     * @param mgr Stream manager to control.
     */
    explicit cli_client(backend::stream_manager& mgr);

    /**
     * @brief Run the interactive command loop.
     *
     * This function reads commands from stdin and prints results/errors to
     * stdout/stderr until the user enters one of the quit commands:
     * "quit", "q", or "exit".
     *
     * @return 0 on normal termination, non-zero if stdin closes / EOF occurs.
     */
    int run() const;

private:
    /**
     * @brief Split a line into whitespace-separated tokens.
     *
     * Used to parse the REPL input into:
     * - command name (first token),
     * - positional/flag arguments (remaining tokens).
     *
     * @param line Raw input line.
     * @return Vector of tokens; empty if line contains no tokens.
     */
    static std::vector<std::string> tokenize(const std::string& line);

    /**
     * @brief Dispatch a command to its handler.
     *
     * Looks up @p cmd in the internal command map and invokes the corresponding
     * cmd_* method. Unknown commands print an error to stderr.
     *
     * @param cmd Command name (e.g., "add-stream").
     * @param args Tokenized arguments excluding the command name.
     */
    void dispatch_command(
        const std::string& cmd, const std::vector<std::string>& args
    ) const;

    /**
     * @brief Parse command arguments using cxxopts.
     *
     * Builds a temporary argv-like array from @p cmd and @p args and calls
     * @ref cxxopts::Options::parse.
     *
     * @param cmd Command name (used as argv[0]).
     * @param args Command arguments.
     * @param options Configured cxxopts options for this command.
     * @return cxxopts parse result.
     */
    static cxxopts::ParseResult parse_with_cxxopts(
        const std::string& cmd, const std::vector<std::string>& args,
        cxxopts::Options& options
    );

    /**
     * @brief Handler for `list-streams`.
     *
     * Supports optional `--connections` to show connected line names.
     *
     * @param args Tokenized arguments.
     */
    void cmd_list_streams(const std::vector<std::string>& args) const;

    /**
     * @brief Handler for `add-stream`.
     *
     * Positional arguments:
     * - path (required)
     * - name (optional)
     * - type (optional: local/file/rtsp/http)
     * - loop (optional: true/false)
     *
     * @param args Tokenized arguments.
     */
    void cmd_add_stream(const std::vector<std::string>& args) const;

    /**
     * @brief Handler for `start-stream`.
     *
     * Positional argument:
     * - name (required)
     *
     * @param args Tokenized arguments.
     */
    void cmd_start_stream(const std::vector<std::string>& args) const;

    /**
     * @brief Handler for `stop-stream`.
     *
     * Positional argument:
     * - name (required)
     *
     * @param args Tokenized arguments.
     */
    void cmd_stop_stream(const std::vector<std::string>& args) const;

    /**
     * @brief Handler for `list-lines`.
     *
     * Lists all stored lines in the manager.
     *
     * @param args Tokenized arguments.
     */
    void cmd_list_lines(const std::vector<std::string>& args) const;

    /**
     * @brief Handler for `add-line`.
     *
     * Positional arguments:
     * - path (required; coordinates string like "0,0;100,100")
     * - name (optional)
     * - close (optional: true/false)
     *
     * Options:
     * - --dir / -d to set tripwire direction.
     *
     * @param args Tokenized arguments.
     */
    void cmd_add_line(const std::vector<std::string>& args) const;

    /**
     * @brief Handler for `set-line`.
     *
     * Positional arguments:
     * - stream (required; stream name)
     * - line (required; line name)
     *
     * Connects an existing line to an existing stream.
     *
     * @param args Tokenized arguments.
     */
    void cmd_set_line(const std::vector<std::string>& args) const;

    /**
     * @brief Stream manager controlled by this CLI.
     *
     * Non-owning reference.
     */
    backend::stream_manager& stream_mgr;
};

} // namespace yodau::backend

#endif // YODAU_BACKEND_CLI_CLIENT_HPP