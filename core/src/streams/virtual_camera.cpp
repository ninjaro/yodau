#include "streams/virtual_camera.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <ostream>
#include <ranges>
#include <string_view>
#include <unordered_set>

#ifdef __linux__
#include <fcntl.h>
#include <filesystem>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace yodau::core {

namespace virtual_camera_support {

    std::string pixel_format_name(const pixel_format format) {
        switch (format) {
        case pixel_format::gray8:
            return "gray8";
        case pixel_format::rgb24:
            return "rgb24";
        case pixel_format::bgr24:
            return "bgr24";
        case pixel_format::rgba32:
            return "rgba32";
        case pixel_format::bgra32:
            return "bgra32";
        }

        return "unknown";
    }

#ifdef __linux__

    constexpr std::uint32_t virtual_camera_pixel_format = V4L2_PIX_FMT_YUYV;

    struct rgb_pixel {
        std::uint8_t red { 0 };
        std::uint8_t green { 0 };
        std::uint8_t blue { 0 };
    };

    struct yuv_pixel {
        std::uint8_t y { 16 };
        std::uint8_t u { 128 };
        std::uint8_t v { 128 };
    };

    struct output_device_candidate {
        std::string path;
        std::string card_name;
    };

    std::string lowercase(std::string text) {
        std::ranges::transform(text, text.begin(), [](const unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return text;
    }

    bool starts_with(
        const std::string_view text, const std::string_view expected_prefix
    ) {
        return text.substr(0, expected_prefix.size()) == expected_prefix;
    }

    bool has_numeric_suffix(
        const std::string_view text, const std::string_view prefix
    ) {
        if (!starts_with(text, prefix) || text.size() <= prefix.size()) {
            return false;
        }

        return std::ranges::all_of(
            text.substr(prefix.size()), [](const char ch) {
                return std::isdigit(static_cast<unsigned char>(ch)) != 0;
            }
        );
    }

    int numeric_suffix_value(
        const std::string_view text, const std::string_view prefix
    ) {
        if (!has_numeric_suffix(text, prefix)) {
            return -1;
        }

        const auto suffix = text.substr(prefix.size());
        int value = 0;
        for (const char ch : suffix) {
            value = value * 10 + (ch - '0');
        }
        return value;
    }

    bool query_output_device(
        const std::string& path, output_device_candidate& out_candidate
    ) {
        const int fd = ::open(path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
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

        // Publication below uses the single-planar VIDEO_OUTPUT structure and
        // write(2). Do not advertise streaming-only or multiplanar devices as
        // compatible until those I/O paths are implemented.
        const bool video_output = (caps & V4L2_CAP_VIDEO_OUTPUT) != 0U;
        const bool io_supported = (caps & V4L2_CAP_READWRITE) != 0U;
        if (!video_output || !io_supported) {
            return false;
        }

        out_candidate.path = path;
        out_candidate.card_name = reinterpret_cast<const char*>(cap.card);
        return true;
    }

    bool is_loopback_like_device(
        const output_device_candidate& candidate, const std::string& camera_name
    ) {
        const std::string lower_path = lowercase(candidate.path);
        const std::string lower_card = lowercase(candidate.card_name);
        const std::string lower_camera_name = lowercase(camera_name);

        return lower_path.find("/dev/" + lower_camera_name + "-video")
            != std::string::npos
            || lower_card.find("loopback") != std::string::npos
            || lower_card.find(lower_camera_name) != std::string::npos;
    }

    std::vector<std::string>
    enumerate_candidate_paths(const std::string& camera_name) {
        std::vector<std::string> preferred_paths;
        std::vector<std::string> fallback_paths;

        const char* explicit_device = std::getenv("YODAU_VCAM_DEVICE");
        if (explicit_device != nullptr && explicit_device[0] != '\0') {
            preferred_paths.emplace_back(explicit_device);
            return preferred_paths;
        }

        const std::string preferred_prefix = camera_name + "-video";

        for (const auto& entry : std::filesystem::directory_iterator("/dev")) {
            if (!entry.is_character_file() && !entry.is_symlink()) {
                continue;
            }

            const std::string name = entry.path().filename().string();
            const std::string full_path = entry.path().string();

            if (has_numeric_suffix(name, preferred_prefix)) {
                preferred_paths.push_back(full_path);
                continue;
            }

            if (has_numeric_suffix(name, "video")) {
                fallback_paths.push_back(full_path);
            }
        }

        auto sort_paths = [&preferred_prefix](std::vector<std::string>& paths) {
            std::ranges::sort(
                paths, [&](const std::string& lhs, const std::string& rhs) {
                    const std::string lhs_name
                        = std::filesystem::path(lhs).filename().string();
                    const std::string rhs_name
                        = std::filesystem::path(rhs).filename().string();
                    const int lhs_value = numeric_suffix_value(
                        lhs_name,
                        starts_with(lhs_name, preferred_prefix)
                            ? preferred_prefix
                            : "video"
                    );
                    const int rhs_value = numeric_suffix_value(
                        rhs_name,
                        starts_with(rhs_name, preferred_prefix)
                            ? preferred_prefix
                            : "video"
                    );
                    return lhs_value < rhs_value;
                }
            );
        };

        sort_paths(preferred_paths);
        sort_paths(fallback_paths);

        preferred_paths.insert(
            preferred_paths.end(), fallback_paths.begin(), fallback_paths.end()
        );
        return preferred_paths;
    }

    std::optional<output_device_candidate> select_output_device(
        const std::string& camera_name,
        const std::unordered_set<std::string>& used_paths
    ) {
        std::vector<output_device_candidate> loopback_fallbacks;

        for (const auto& path : enumerate_candidate_paths(camera_name)) {
            if (used_paths.contains(path)) {
                continue;
            }

            output_device_candidate candidate;
            if (!query_output_device(path, candidate)) {
                continue;
            }

            const std::string file_name
                = std::filesystem::path(path).filename().string();
            const bool preferred_name
                = has_numeric_suffix(file_name, camera_name + "-video");
            if (preferred_name) {
                return candidate;
            }

            if (is_loopback_like_device(candidate, camera_name)) {
                loopback_fallbacks.push_back(std::move(candidate));
            }
        }

        if (!loopback_fallbacks.empty()) {
            return loopback_fallbacks.front();
        }

        return {};
    }

    std::string last_errno_text(const std::string& prefix) {
        return prefix + ": " + std::strerror(errno);
    }

    bool same_output_device(const std::string& left, const std::string& right) {
        if (left == right) {
            return true;
        }
        std::error_code error;
        return std::filesystem::equivalent(left, right, error) && !error;
    }

    int normalized_output_width(const int width) {
        if (width <= 1) {
            return 2;
        }

        return (width % 2 == 0) ? width : width - 1;
    }

    rgb_pixel sample_rgb_pixel(
        const frame& frame_value, const int out_x, const int out_y,
        const int out_width, const int out_height
    ) {
        rgb_pixel pixel;

        if (frame_value.width <= 0 || frame_value.height <= 0
            || frame_value.stride <= 0 || frame_value.data.empty()
            || out_width <= 0 || out_height <= 0) {
            return pixel;
        }

        const auto scaled_coordinate = [](const int output_coordinate,
                                          const int source_extent,
                                          const int output_extent) {
            const auto scaled = static_cast<std::uint64_t>(output_coordinate)
                * static_cast<std::uint64_t>(source_extent)
                / static_cast<std::uint64_t>(output_extent);
            return static_cast<int>(
                std::min(scaled, static_cast<std::uint64_t>(source_extent - 1))
            );
        };
        const int src_x
            = scaled_coordinate(out_x, frame_value.width, out_width);
        const int src_y
            = scaled_coordinate(out_y, frame_value.height, out_height);

        const size_t row_offset = static_cast<size_t>(src_y)
            * static_cast<size_t>(frame_value.stride);

        switch (frame_value.format) {
        case pixel_format::gray8: {
            const size_t offset = row_offset + static_cast<size_t>(src_x);
            if (offset >= frame_value.data.size()) {
                return pixel;
            }
            const std::uint8_t value = frame_value.data[offset];
            pixel.red = value;
            pixel.green = value;
            pixel.blue = value;
            return pixel;
        }
        case pixel_format::rgb24: {
            const size_t offset = row_offset + static_cast<size_t>(src_x) * 3u;
            if (offset + 2u >= frame_value.data.size()) {
                return pixel;
            }
            pixel.red = frame_value.data[offset];
            pixel.green = frame_value.data[offset + 1u];
            pixel.blue = frame_value.data[offset + 2u];
            return pixel;
        }
        case pixel_format::bgr24: {
            const size_t offset = row_offset + static_cast<size_t>(src_x) * 3u;
            if (offset + 2u >= frame_value.data.size()) {
                return pixel;
            }
            pixel.blue = frame_value.data[offset];
            pixel.green = frame_value.data[offset + 1u];
            pixel.red = frame_value.data[offset + 2u];
            return pixel;
        }
        case pixel_format::rgba32: {
            const size_t offset = row_offset + static_cast<size_t>(src_x) * 4u;
            if (offset + 2u >= frame_value.data.size()) {
                return pixel;
            }
            pixel.red = frame_value.data[offset];
            pixel.green = frame_value.data[offset + 1u];
            pixel.blue = frame_value.data[offset + 2u];
            return pixel;
        }
        case pixel_format::bgra32: {
            const size_t offset = row_offset + static_cast<size_t>(src_x) * 4u;
            if (offset + 2u >= frame_value.data.size()) {
                return pixel;
            }
            pixel.blue = frame_value.data[offset];
            pixel.green = frame_value.data[offset + 1u];
            pixel.red = frame_value.data[offset + 2u];
            return pixel;
        }
        }

        return pixel;
    }

    std::uint8_t clamp_byte(const int value) {
        return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
    }

    yuv_pixel rgb_to_yuv(const rgb_pixel& pixel) {
        const int red = static_cast<int>(pixel.red);
        const int green = static_cast<int>(pixel.green);
        const int blue = static_cast<int>(pixel.blue);

        yuv_pixel converted;
        converted.y = clamp_byte(
            ((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16
        );
        converted.u = clamp_byte(
            ((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128
        );
        converted.v = clamp_byte(
            ((112 * red - 94 * green - 18 * blue + 128) >> 8) + 128
        );
        return converted;
    }

    std::vector<std::uint8_t> frame_to_yuyv(
        const frame& frame_value, const int output_width,
        const int output_height, const size_t output_stride,
        const size_t output_size
    ) {
        const int width = normalized_output_width(output_width);
        if (width <= 0 || output_height <= 0) {
            return {};
        }

        const size_t row_bytes = static_cast<size_t>(width) * 2u;
        const auto height = static_cast<size_t>(output_height);
        if (output_stride < row_bytes || height > output_size / output_stride) {
            return {};
        }

        std::vector<std::uint8_t> output(output_size, 0u);

        for (int y = 0; y < output_height; ++y) {
            size_t write_index = static_cast<size_t>(y) * output_stride;
            for (int x = 0; x < width; x += 2) {
                const rgb_pixel left_rgb
                    = sample_rgb_pixel(frame_value, x, y, width, output_height);
                const rgb_pixel right_rgb = sample_rgb_pixel(
                    frame_value, x + 1, y, width, output_height
                );
                const yuv_pixel left_yuv = rgb_to_yuv(left_rgb);
                const yuv_pixel right_yuv = rgb_to_yuv(right_rgb);

                const int averaged_u = (static_cast<int>(left_yuv.u)
                                        + static_cast<int>(right_yuv.u))
                    / 2;
                const int averaged_v = (static_cast<int>(left_yuv.v)
                                        + static_cast<int>(right_yuv.v))
                    / 2;

                output[write_index++] = left_yuv.y;
                output[write_index++] = clamp_byte(averaged_u);
                output[write_index++] = right_yuv.y;
                output[write_index++] = clamp_byte(averaged_v);
            }
        }

        return output;
    }

    bool configure_output_device(
        const int fd, const frame& frame_value, int& out_width, int& out_height,
        size_t& out_bytes_per_line, size_t& out_bytes_per_frame,
        std::string& out_error
    ) {
        v4l2_format fmt {};
        fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        fmt.fmt.pix.width = static_cast<std::uint32_t>(
            normalized_output_width(frame_value.width)
        );
        fmt.fmt.pix.height
            = static_cast<std::uint32_t>(std::max(1, frame_value.height));
        fmt.fmt.pix.pixelformat = virtual_camera_pixel_format;
        fmt.fmt.pix.field = V4L2_FIELD_NONE;
        fmt.fmt.pix.bytesperline = fmt.fmt.pix.width * 2u;
        fmt.fmt.pix.sizeimage = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
        fmt.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;

        if (::ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
            out_error = last_errno_text("VIDIOC_S_FMT failed");
            return false;
        }

        if (fmt.fmt.pix.pixelformat != virtual_camera_pixel_format) {
            out_error = "virtual camera refused YUYV output format";
            return false;
        }

        out_width = static_cast<int>(fmt.fmt.pix.width);
        out_height = static_cast<int>(fmt.fmt.pix.height);
        out_bytes_per_line = static_cast<size_t>(fmt.fmt.pix.bytesperline);
        out_bytes_per_frame = static_cast<size_t>(fmt.fmt.pix.sizeimage);
        const size_t required_row_bytes = static_cast<size_t>(out_width) * 2u;
        const auto height = static_cast<size_t>(out_height);
        if (out_bytes_per_line < required_row_bytes
            || height > out_bytes_per_frame / out_bytes_per_line) {
            out_error
                = "virtual camera returned an invalid padded frame layout";
            return false;
        }
        out_error.clear();
        return true;
    }

#endif

    std::string
    sink_status_text(const bool device_ready, const std::string& last_error) {
        if (device_ready) {
            return "ready";
        }

        if (!last_error.empty()) {
            return last_error;
        }

        return "unavailable";
    }

} // namespace virtual_camera_support

virtual_camera::virtual_camera(
    std::string virtual_camera_name, std::string device_path
)
    : camera_name(std::move(virtual_camera_name))
    , requested_device_path(std::move(device_path)) { }

virtual_camera::~virtual_camera() {
    std::unordered_map<std::string, std::shared_ptr<sink_binding>> sinks;
    {
        std::scoped_lock lock(mtx);
        sinks.swap(sink_by_stream);
    }
    for (const auto& [stream_name, binding] : sinks) {
        (void)stream_name;
        stop_sink(binding);
    }
}

void virtual_camera::publish(
    const std::string& stream_name, const frame& frame_value
) {
    const frame_layout_validation layout = validate_frame_layout(frame_value);
    if (!layout) {
        std::shared_ptr<sink_binding> invalid_binding;
        {
            std::scoped_lock lock(mtx);
            invalid_binding = ensure_binding_locked(stream_name);
        }
        {
            std::scoped_lock lock(invalid_binding->mtx);
            invalid_binding->last_error = "invalid source frame: "
                + std::string(frame_layout_error_message(layout.error));
            ++invalid_binding->dropped_frame_count;
        }
        return;
    }

    auto frame_snapshot = std::make_shared<const frame>(frame_value);
    std::shared_ptr<sink_binding> binding;
    {
        std::scoped_lock lock(mtx);
        latest_by_stream[stream_name] = frame_snapshot;
        update_count_by_stream[stream_name] += 1;
        binding = ensure_binding_locked(stream_name);
    }

    {
        std::scoped_lock lock(binding->mtx);
        if (binding->pending_frame) {
            ++binding->dropped_frame_count;
        }
        binding->pending_frame = std::move(frame_snapshot);
    }
    binding->wake.notify_one();
}

void virtual_camera::release(const std::string& stream_name) {
    std::shared_ptr<sink_binding> binding;
    {
        std::scoped_lock lock(mtx);
        if (const auto sink_it = sink_by_stream.find(stream_name);
            sink_it != sink_by_stream.end()) {
            binding = std::move(sink_it->second);
            sink_by_stream.erase(sink_it);
        }

        latest_by_stream.erase(stream_name);
        update_count_by_stream.erase(stream_name);
    }

    stop_sink(binding);
}

std::optional<frame>
virtual_camera::latest_frame(const std::string& stream_name) const {
    std::scoped_lock lock(mtx);

    const auto frame_it = latest_by_stream.find(stream_name);
    if (frame_it == latest_by_stream.end()) {
        return {};
    }

    return *frame_it->second;
}

std::vector<virtual_camera_frame_info> virtual_camera::frames() const {
    std::vector<virtual_camera_frame_info> out;

    std::scoped_lock lock(mtx);
    out.reserve(sink_by_stream.size());

    for (const auto& [stream_name, binding] : sink_by_stream) {
        const auto frame_it = latest_by_stream.find(stream_name);
        const auto update_it = update_count_by_stream.find(stream_name);

        virtual_camera_frame_info info;
        info.stream_name = stream_name;
        if (frame_it != latest_by_stream.end()) {
            info.width = frame_it->second->width;
            info.height = frame_it->second->height;
            info.format = frame_it->second->format;
            info.bytes = frame_it->second->data.size();
        }
        if (update_it != update_count_by_stream.end()) {
            info.update_count = update_it->second;
        }
        if (binding) {
            std::scoped_lock sink_lock(binding->mtx);
            info.device_path = binding->device_path;
            info.device_ready
                = binding->device_ready && binding->published_frame_count > 0U;
            info.last_error = binding->last_error;
            info.published_frame_count = binding->published_frame_count;
            info.dropped_frame_count = binding->dropped_frame_count;
        }

        out.push_back(std::move(info));
    }

    std::ranges::sort(
        out, std::less<> {}, &virtual_camera_frame_info::stream_name
    );
    return out;
}

void virtual_camera::dump(std::ostream& out) const {
    const auto snapshots = frames();
    out << snapshots.size() << " virtual camera streams:";

    for (const auto& info : snapshots) {
        out << "\n\tVirtualCamera(stream=" << info.stream_name << ", device="
            << (info.device_path.empty() ? "<none>" : info.device_path)
            << ", state="
            << virtual_camera_support::sink_status_text(
                   info.device_ready, info.last_error
               )
            << ", frame=" << info.width << "x" << info.height << ", format="
            << virtual_camera_support::pixel_format_name(info.format)
            << ", bytes=" << info.bytes << ", updates=" << info.update_count
            << ", published=" << info.published_frame_count
            << ", dropped=" << info.dropped_frame_count << ")";
    }
}

std::shared_ptr<virtual_camera::sink_binding>
virtual_camera::ensure_binding_locked(const std::string& stream_name) {
    if (const auto it = sink_by_stream.find(stream_name);
        it != sink_by_stream.end()) {
        return it->second;
    }

    auto binding = std::make_shared<sink_binding>();
#ifdef __linux__
    binding->device_path = requested_device_path;
    if (!binding->device_path.empty()) {
        if (!sink_by_stream.empty()) {
            binding->last_error = "explicit virtual camera device is already "
                                  "assigned to stream '"
                + sink_by_stream.begin()->first + "'";
            binding->device_path.clear();
        }
        for (const auto& [other_stream_name, other_binding] : sink_by_stream) {
            if (other_stream_name == stream_name || !other_binding) {
                continue;
            }
            std::scoped_lock other_lock(other_binding->mtx);
            if (!other_binding->device_path.empty()
                && virtual_camera_support::same_output_device(
                    binding->device_path, other_binding->device_path
                )) {
                binding->last_error = "explicit virtual camera device is "
                                      "already assigned to stream '"
                    + other_stream_name + "'";
                binding->device_path.clear();
                break;
            }
        }
        if (!binding->device_path.empty()) {
            virtual_camera_support::output_device_candidate candidate;
            if (!virtual_camera_support::query_output_device(
                    binding->device_path, candidate
                )) {
                binding->last_error
                    = "configured virtual camera device must "
                      "support single-planar V4L2 video-output write I/O";
                binding->device_path.clear();
            }
        }
    } else {
        std::unordered_set<std::string> used_paths;
        for (const auto& [other_stream_name, other_binding] : sink_by_stream) {
            if (other_stream_name == stream_name || !other_binding) {
                continue;
            }
            std::scoped_lock other_lock(other_binding->mtx);
            if (!other_binding->device_path.empty()) {
                used_paths.insert(other_binding->device_path);
            }
        }

        const auto candidate = virtual_camera_support::select_output_device(
            camera_name, used_paths
        );
        if (candidate.has_value()) {
            binding->device_path = candidate->path;
        } else {
            binding->last_error
                = "no compatible Linux virtual camera device available";
        }
    }
#else
    binding->last_error
        = "Linux virtual camera output is unavailable on this platform";
#endif

    binding->worker = stoppable_thread([binding](const stop_token& stop_token) {
        try {
            run_sink(binding, stop_token);
        } catch (const std::exception& error) {
            std::scoped_lock lock(binding->mtx);
            binding->device_ready = false;
            binding->last_error
                = "virtual camera worker failed: " + std::string(error.what());
        } catch (...) {
            std::scoped_lock lock(binding->mtx);
            binding->device_ready = false;
            binding->last_error
                = "virtual camera worker failed with an unknown exception";
        }
    });
    sink_by_stream.emplace(stream_name, binding);
    return binding;
}

void virtual_camera::run_sink(
    const std::shared_ptr<sink_binding>& binding, const stop_token& stop_token
) {
    while (!stop_token.stop_requested()) {
        std::shared_ptr<const frame> next_frame;
        {
            std::unique_lock lock(binding->mtx);
            binding->wake.wait(lock, [&] {
                return stop_token.stop_requested()
                    || static_cast<bool>(binding->pending_frame);
            });
            if (stop_token.stop_requested()) {
                break;
            }
            next_frame = std::move(binding->pending_frame);
            binding->pending_frame.reset();
        }

#ifdef __linux__
        int fd = -1;
        int output_width = 0;
        int output_height = 0;
        size_t bytes_per_line = 0;
        size_t bytes_per_frame = 0;
        {
            std::scoped_lock lock(binding->mtx);
            if (next_frame->width <= 0 || next_frame->height <= 0
                || next_frame->data.empty()) {
                binding->last_error = "stream frame is empty";
                ++binding->dropped_frame_count;
                continue;
            }

            const int required_width
                = virtual_camera_support::normalized_output_width(
                    next_frame->width
                );
            const int required_height = std::max(1, next_frame->height);
            if (!binding->device_ready || binding->fd < 0
                || binding->width != required_width
                || binding->height != required_height) {
                close_sink(*binding);
                if (binding->device_path.empty()) {
                    binding->last_error
                        = "no compatible Linux virtual camera device available";
                    ++binding->dropped_frame_count;
                    continue;
                }

                const int opened_fd = ::open(
                    binding->device_path.c_str(),
                    O_RDWR | O_NONBLOCK | O_CLOEXEC
                );
                if (opened_fd < 0) {
                    binding->last_error
                        = virtual_camera_support::last_errno_text(
                            "opening virtual camera device failed"
                        );
                    ++binding->dropped_frame_count;
                    continue;
                }

                std::string error_text;
                if (!virtual_camera_support::configure_output_device(
                        opened_fd, *next_frame, binding->width, binding->height,
                        binding->bytes_per_line, binding->bytes_per_frame,
                        error_text
                    )) {
                    ::close(opened_fd);
                    binding->last_error = std::move(error_text);
                    ++binding->dropped_frame_count;
                    continue;
                }
                binding->fd = opened_fd;
                binding->device_ready = true;
                binding->last_error.clear();
            }

            fd = binding->fd;
            output_width = binding->width;
            output_height = binding->height;
            bytes_per_line = binding->bytes_per_line;
            bytes_per_frame = binding->bytes_per_frame;
        }

        std::vector<std::uint8_t> output_frame
            = virtual_camera_support::frame_to_yuyv(
                *next_frame, output_width, output_height, bytes_per_line,
                bytes_per_frame
            );

        std::scoped_lock lock(binding->mtx);
        if (!binding->device_ready || binding->fd != fd) {
            ++binding->dropped_frame_count;
            continue;
        }
        if (output_frame.size() != bytes_per_frame) {
            binding->last_error
                = "virtual camera frame size does not match sink format";
            ++binding->dropped_frame_count;
            close_sink(*binding);
            continue;
        }

        const ssize_t written
            = ::write(fd, output_frame.data(), output_frame.size());
        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            ++binding->dropped_frame_count;
            continue;
        }
        if (written < 0) {
            binding->last_error = virtual_camera_support::last_errno_text(
                "writing virtual camera frame failed"
            );
            ++binding->dropped_frame_count;
            close_sink(*binding);
            continue;
        }
        if (static_cast<size_t>(written) != output_frame.size()) {
            binding->last_error = "virtual camera frame write was truncated";
            ++binding->dropped_frame_count;
            close_sink(*binding);
            continue;
        }
        ++binding->published_frame_count;
        binding->last_error.clear();
#else
        std::scoped_lock lock(binding->mtx);
        ++binding->dropped_frame_count;
#endif
    }
}

void virtual_camera::stop_sink(const std::shared_ptr<sink_binding>& binding) {
    if (!binding) {
        return;
    }
    if (binding->worker.joinable()) {
        binding->worker.request_stop();
        binding->wake.notify_all();
        binding->worker.join();
    }
    std::scoped_lock lock(binding->mtx);
    binding->pending_frame.reset();
    close_sink(*binding);
}

void virtual_camera::close_sink(sink_binding& binding) {
#ifdef __linux__
    if (binding.fd >= 0) {
        ::close(binding.fd);
    }
    binding.fd = -1;
    binding.width = 0;
    binding.height = 0;
    binding.bytes_per_line = 0;
    binding.bytes_per_frame = 0;
#endif
    binding.device_ready = false;
    binding.published_frame_count = 0U;
}

} // namespace yodau::core
