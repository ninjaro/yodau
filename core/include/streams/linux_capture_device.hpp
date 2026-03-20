#ifndef YODAU_BACKEND_LINUX_CAPTURE_DEVICE_HPP
#define YODAU_BACKEND_LINUX_CAPTURE_DEVICE_HPP

#include <string>
#include <vector>

namespace yodau::backend {

bool is_linux_capture_device(const std::string& path);
std::vector<std::string> list_linux_capture_devices();

} // namespace yodau::backend

#endif // YODAU_BACKEND_LINUX_CAPTURE_DEVICE_HPP
