#ifndef YODAU_CORE_LINUX_CAPTURE_DEVICE_HPP
#define YODAU_CORE_LINUX_CAPTURE_DEVICE_HPP

#include "core/namespace_alias.hpp"
#include <string>
#include <vector>

namespace yodau::core {

bool is_linux_capture_device(const std::string& path);
std::vector<std::string> list_linux_capture_devices();

} // namespace yodau::core

#endif // YODAU_CORE_LINUX_CAPTURE_DEVICE_HPP
