#include "streams/apple_capture_device.hpp"

#include "streams/stream.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <stdexcept>

std::string_view yodau::core::apple_capture_authorization_name(
    const apple_capture_authorization status
) noexcept {
    switch (status) {
    case apple_capture_authorization::unavailable:
        return "unavailable";
    case apple_capture_authorization::not_determined:
        return "not_determined";
    case apple_capture_authorization::restricted:
        return "restricted";
    case apple_capture_authorization::denied:
        return "denied";
    case apple_capture_authorization::authorized:
        return "authorized";
    }
    return "unknown";
}

std::optional<std::string>
yodau::core::apple_capture_device_id_from_path(const std::string_view path) {
    if (!path.starts_with(apple_capture_path_prefix)) {
        return std::nullopt;
    }
    const std::string_view id = path.substr(apple_capture_path_prefix.size());
    if (id.empty()) {
        return std::nullopt;
    }
    return std::string(id);
}

std::string yodau::core::apple_capture_path_for_device_id(
    const std::string_view unique_id
) {
    return std::string(apple_capture_path_prefix) + std::string(unique_id);
}

std::string yodau::core::apple_capture_stream_name(
    const std::string_view device_name, const std::vector<std::string>& taken
) {
    std::string base;
    for (const char raw : device_name) {
        const auto ch = static_cast<unsigned char>(raw);
        if (std::isalnum(ch) != 0) {
            base.push_back(static_cast<char>(std::tolower(ch)));
        } else if (!base.empty() && base.back() != '-') {
            base.push_back('-');
        }
    }
    while (!base.empty() && base.back() == '-') {
        base.pop_back();
    }
    if (base.empty()) {
        base = "camera";
    }

    std::string name = base;
    for (int suffix = 2; std::ranges::find(taken, name) != taken.end();
         ++suffix) {
        name = base + "-" + std::to_string(suffix);
    }
    return name;
}

std::optional<std::string> yodau::core::resolve_apple_capture_device(
    const std::string_view path,
    const std::vector<apple_capture_device_info>& devices
) {
    const auto connected = [](const apple_capture_device_info& device) {
        return device.connected;
    };

    std::string_view id = path;
    if (const auto explicit_id = apple_capture_device_id_from_path(path)) {
        const auto it = std::ranges::find_if(
            devices, [&](const apple_capture_device_info& device) {
                return device.unique_id == *explicit_id && connected(device);
            }
        );
        if (it == devices.end()) {
            return std::nullopt;
        }
        return it->unique_id;
    }

    constexpr std::string_view legacy_prefix = "/dev/video";
    if (path.starts_with(legacy_prefix)) {
        const std::string_view tail = path.substr(legacy_prefix.size());
        std::size_t index = 0;
        const auto res
            = std::from_chars(tail.data(), tail.data() + tail.size(), index);
        if (tail.empty() || res.ec != std::errc()
            || res.ptr != tail.data() + tail.size()) {
            return std::nullopt;
        }
        std::size_t seen = 0;
        for (const auto& device : devices) {
            if (!connected(device)) {
                continue;
            }
            if (seen == index) {
                return device.unique_id;
            }
            ++seen;
        }
        return std::nullopt;
    }

    const auto it = std::ranges::find_if(
        devices, [&](const apple_capture_device_info& device) {
            return device.unique_id == id && connected(device);
        }
    );
    if (it == devices.end()) {
        return std::nullopt;
    }
    return it->unique_id;
}

std::optional<std::string>
yodau::core::resolve_apple_capture_device(const std::string_view path) {
    return resolve_apple_capture_device(path, list_apple_capture_devices());
}

bool yodau::core::uses_apple_capture(const stream& stream_value) {
    return apple_capture_available() && stream_value.get_type() == local;
}

// Canonical builds do not yet supply Objective-C++, ARC or Apple frameworks.
// Keep the complete unavailable backend on every platform until that contract
// is generated and verified; __APPLE__ alone cannot establish availability.

bool yodau::core::apple_capture_available() noexcept { return false; }

yodau::core::apple_capture_authorization
yodau::core::apple_capture_authorization_status() {
    return apple_capture_authorization::unavailable;
}

yodau::core::apple_capture_authorization
yodau::core::request_apple_capture_authorization(
    const std::chrono::milliseconds timeout
) {
    static_cast<void>(timeout);
    return apple_capture_authorization::unavailable;
}

std::vector<yodau::core::apple_capture_device_info>
yodau::core::list_apple_capture_devices() {
    return {};
}

void yodau::core::run_apple_capture(
    const std::string& path, const std::function<void(frame&&)>& on_frame,
    const stop_token& token, const apple_capture_options& options
) {
    static_cast<void>(path);
    static_cast<void>(on_frame);
    static_cast<void>(token);
    static_cast<void>(options);
    throw std::runtime_error(
        "AVFoundation capture is unavailable in this build"
    );
}
