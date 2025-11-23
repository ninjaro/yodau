#include "cli_client.hpp"

int main() {
    yodau::backend::stream_manager stream_mgr {};
    const yodau::backend::cli_client client(stream_mgr);
    return client.run();
}