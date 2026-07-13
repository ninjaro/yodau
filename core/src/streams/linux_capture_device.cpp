#include "streams/linux_capture_device.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <system_error>

#ifdef __linux__
#include <fcntl.h>
#include <filesystem>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

bool yodau::core::is_linux_capture_device(const std::string& path) {
#ifdef __linux__
    const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
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

yodau::core::linux_capture_exclusivity_result
yodau::core::probe_linux_capture_exclusivity(const std::string& path) {
#ifdef __linux__
    const int fd = ::open(path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        if (errno == EBUSY) {
            return {
                linux_capture_exclusivity::exclusive,
                "driver rejected a second open with EBUSY",
            };
        }
        return {
            linux_capture_exclusivity::indeterminate,
            "second-open probe failed: " + std::string(std::strerror(errno)),
        };
    }

    v4l2_capability cap {};
    if (::ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        const std::string detail = "VIDIOC_QUERYCAP probe failed: "
            + std::string(std::strerror(errno));
        ::close(fd);
        return { linux_capture_exclusivity::indeterminate, detail };
    }

    std::uint32_t caps = cap.capabilities;
    if ((caps & V4L2_CAP_DEVICE_CAPS) != 0U) {
        caps = cap.device_caps;
    }
    v4l2_requestbuffers request {};
    request.count = 1;
    request.memory = V4L2_MEMORY_MMAP;
    request.type = (caps & V4L2_CAP_VIDEO_CAPTURE) != 0U
        ? V4L2_BUF_TYPE_VIDEO_CAPTURE
        : V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    if (::ioctl(fd, VIDIOC_REQBUFS, &request) < 0) {
        const int request_errno = errno;
        ::close(fd);
        if (request_errno == EBUSY) {
            return {
                linux_capture_exclusivity::exclusive,
                "driver rejected a second V4L2 buffer owner with EBUSY",
            };
        }
        return {
            linux_capture_exclusivity::indeterminate,
            "second-buffer-owner probe failed without EBUSY: "
                + std::string(std::strerror(request_errno)),
        };
    }

    v4l2_requestbuffers release = request;
    release.count = 0;
    (void)::ioctl(fd, VIDIOC_REQBUFS, &release);
    ::close(fd);
    return {
        linux_capture_exclusivity::shared,
        "driver allowed a second V4L2 buffer owner while capture was active",
    };
#else
    static_cast<void>(path);
    return {
        linux_capture_exclusivity::indeterminate,
        "V4L2 exclusivity probing is unavailable on this platform",
    };
#endif
}

std::vector<std::string> yodau::core::list_linux_capture_devices() {
    std::vector<std::string> paths;

#ifdef __linux__
    for (std::string path : list_linux_capture_device_candidates()) {
        if (!is_linux_capture_device(path)) {
            continue;
        }
        paths.push_back(std::move(path));
    }
#endif

    return paths;
}

std::vector<std::string> yodau::core::list_linux_capture_device_candidates(
    const std::filesystem::path& device_directory
) {
    std::vector<std::string> paths;

#ifdef __linux__
    std::error_code error;
    std::filesystem::directory_iterator entries(device_directory, error);
    if (error) {
        return paths;
    }

    for (const auto& entry : entries) {
        const std::string filename = entry.path().filename().string();
        constexpr std::string_view prefix = "video";
        if (!filename.starts_with(prefix) || filename.size() == prefix.size()) {
            continue;
        }

        const std::string_view suffix(
            filename.data() + prefix.size(), filename.size() - prefix.size()
        );
        if (!std::ranges::all_of(suffix, [](const unsigned char ch) {
                return std::isdigit(ch) != 0;
            })) {
            continue;
        }
        paths.push_back(entry.path().string());
    }

    std::ranges::sort(
        paths, [](const std::string& lhs, const std::string& rhs) {
            const std::string lhs_name
                = std::filesystem::path(lhs).filename().string();
            const std::string rhs_name
                = std::filesystem::path(rhs).filename().string();
            const std::string_view lhs_suffix(
                lhs_name.data() + 5, lhs_name.size() - 5
            );
            const std::string_view rhs_suffix(
                rhs_name.data() + 5, rhs_name.size() - 5
            );
            if (lhs_suffix.size() != rhs_suffix.size()) {
                return lhs_suffix.size() < rhs_suffix.size();
            }
            return lhs_suffix < rhs_suffix;
        }
    );
#else
    static_cast<void>(device_directory);
#endif

    return paths;
}
