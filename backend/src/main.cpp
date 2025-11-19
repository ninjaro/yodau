#include "cli_client.hpp"

int main() {
    yodau::backend::cli_client client;
    return client.run();
}