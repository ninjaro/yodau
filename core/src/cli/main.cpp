#include "cli/cli_client.hpp"

int main() {
    yodau::core::stream_manager stream_mgr {};
    yodau::core::cli_client client(stream_mgr);
    return client.run();
}
