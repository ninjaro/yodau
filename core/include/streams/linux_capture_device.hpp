#ifndef YODAU_CORE_LINUX_CAPTURE_DEVICE_HPP
#define YODAU_CORE_LINUX_CAPTURE_DEVICE_HPP

#include "core/namespace_alias.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace yodau::core {

enum class linux_capture_exclusivity {
    exclusive,
    shared,
    indeterminate,
};

struct linux_capture_exclusivity_result {
    linux_capture_exclusivity state {
        linux_capture_exclusivity::indeterminate
    };
    std::string detail;
};

bool is_linux_capture_device(const std::string& path);
// Probe a second V4L2 file handle after capture has started. `exclusive` means
// the driver rejected a second buffer owner with EBUSY. Callers requiring hard
// source isolation must fail closed for every other result.
linux_capture_exclusivity_result
probe_linux_capture_exclusivity(const std::string& path);
std::vector<std::string> list_linux_capture_device_candidates(
    const std::filesystem::path& device_directory = "/dev"
);
std::vector<std::string> list_linux_capture_devices();

} // namespace yodau::core

#endif // YODAU_CORE_LINUX_CAPTURE_DEVICE_HPP
