#include "cli/cli_client.hpp"
#include "monitor/client.hpp"

int main() {
    static_cast<void>(monitor::start_process("yodau-cli"));
    yodau::core::stream_manager stream_mgr {};
    yodau::core::cli_client client(stream_mgr);
    return client.run();
}
