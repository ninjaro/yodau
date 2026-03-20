#include "streams/linux_capture_device.hpp"

#ifdef __linux__
#include <filesystem>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

bool yodau::backend::is_linux_capture_device(const std::string& path) {
#ifdef __linux__
    const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        return false;
    }

    v4l2_capability cap {};
    const int rc = ::ioctl(fd, VIDIOC_QUERYCAP, &cap);
    ::close(fd);

    if (rc < 0) {
        return false;
    }

    std::uint32_t caps = cap.capabilities;
    if (caps & V4L2_CAP_DEVICE_CAPS) {
        caps = cap.device_caps;
    }

    const bool capture = (caps & V4L2_CAP_VIDEO_CAPTURE)
        || (caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE);
    const bool streaming = (caps & V4L2_CAP_STREAMING);
    return capture && streaming;
#else
    static_cast<void>(path);
    return false;
#endif
}

std::vector<std::string> yodau::backend::list_linux_capture_devices() {
    std::vector<std::string> paths;

#ifdef __linux__
    for (size_t idx = 0;; ++idx) {
        std::string path = "/dev/video" + std::to_string(idx);
        if (!std::filesystem::exists(path)) {
            break;
        }
        if (!is_linux_capture_device(path)) {
            continue;
        }
        paths.push_back(std::move(path));
    }
#endif

    return paths;
}
