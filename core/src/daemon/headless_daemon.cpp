#include "daemon/headless_daemon.hpp"

#include "analysis/processing_runtime.hpp"
#include "configuration/line_configuration_file.hpp"
#include "streams/linux_capture_device.hpp"
#include "streams/stream_manager.hpp"
#include "streams/virtual_camera.hpp"

#include "monitor/client.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <ostream>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <utility>

#ifdef __linux__
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#endif

namespace yodau::core {
namespace {
    class source_camera_lease {
    public:
        source_camera_lease() = default;

        ~source_camera_lease() { reset(); }

        source_camera_lease(const source_camera_lease&) = delete;
        source_camera_lease& operator=(const source_camera_lease&) = delete;

        source_camera_lease(source_camera_lease&& other) noexcept
#ifdef __linux__
            : fd_(std::exchange(other.fd_, -1))
            , is_capture_device_(std::exchange(other.is_capture_device_, false))
#endif
        {
        }

        source_camera_lease& operator=(source_camera_lease&& other) noexcept {
            if (this == &other) {
                return *this;
            }
            reset();
#ifdef __linux__
            fd_ = std::exchange(other.fd_, -1);
            is_capture_device_ = std::exchange(other.is_capture_device_, false);
#endif
            return *this;
        }

        static source_camera_lease
        acquire(const std::string& source, const bool require_capture_device) {
            source_camera_lease lease;
#ifdef __linux__
            if (!require_capture_device) {
                return lease;
            }
            lease.is_capture_device_ = true;

            const int source_fd
                = ::open(source.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (source_fd < 0) {
                throw std::runtime_error(
                    "cannot open source camera '" + source
                    + "': " + std::strerror(errno)
                );
            }
            struct stat source_stat {};
            if (::fstat(source_fd, &source_stat) < 0) {
                const std::string message = "cannot identify source camera '"
                    + source + "': " + std::strerror(errno);
                ::close(source_fd);
                throw std::runtime_error(message);
            }
            v4l2_capability capabilities {};
            if (::ioctl(source_fd, VIDIOC_QUERYCAP, &capabilities) < 0) {
                ::close(source_fd);
                throw std::runtime_error(
                    "configured local source does not provide V4L2 "
                    "capabilities"
                );
            }
            std::uint32_t capability_flags = capabilities.capabilities;
            if ((capability_flags & V4L2_CAP_DEVICE_CAPS) != 0U) {
                capability_flags = capabilities.device_caps;
            }
            const bool supports_capture
                = (capability_flags & V4L2_CAP_VIDEO_CAPTURE) != 0U
                || (capability_flags & V4L2_CAP_VIDEO_CAPTURE_MPLANE) != 0U;
            if (!supports_capture
                || (capability_flags & V4L2_CAP_STREAMING) == 0U) {
                ::close(source_fd);
                throw std::runtime_error(
                    "configured local source is not a streaming V4L2 "
                    "capture device"
                );
            }
            ::close(source_fd);

            const uid_t uid = ::getuid();
            std::filesystem::path lock_directory
                = "/run/user/" + std::to_string(uid);
            std::error_code directory_error;
            if (!std::filesystem::is_directory(
                    lock_directory, directory_error
                )) {
                lock_directory = std::filesystem::temp_directory_path()
                    / ("yodau-" + std::to_string(uid));
                if (::mkdir(lock_directory.c_str(), S_IRWXU) < 0
                    && errno != EEXIST) {
                    throw std::runtime_error(
                        "cannot create private source-camera lock directory: "
                        + std::string(std::strerror(errno))
                    );
                }
                struct stat lock_directory_status {};
                if (::lstat(lock_directory.c_str(), &lock_directory_status) < 0
                    || !S_ISDIR(lock_directory_status.st_mode)
                    || lock_directory_status.st_uid != uid) {
                    throw std::runtime_error(
                        "source-camera lock directory is not a private "
                        "directory owned by the daemon user"
                    );
                }
                if ((lock_directory_status.st_mode & (S_IRWXG | S_IRWXO)) != 0U
                    && ::chmod(lock_directory.c_str(), S_IRWXU) < 0) {
                    throw std::runtime_error(
                        "cannot secure source-camera lock directory: "
                        + std::string(std::strerror(errno))
                    );
                }
            }
            const std::string lock_name = "yodau-" + std::to_string(uid)
                + "-camera-" + std::to_string(major(source_stat.st_rdev)) + "-"
                + std::to_string(minor(source_stat.st_rdev)) + ".lock";
            const std::filesystem::path lock_path = lock_directory / lock_name;

            lease.fd_ = ::open(
                lock_path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW,
                S_IRUSR | S_IWUSR
            );
            if (lease.fd_ < 0) {
                throw std::runtime_error(
                    "cannot open source-camera lease '" + lock_path.string()
                    + "': " + std::strerror(errno)
                );
            }
            if (::flock(lease.fd_, LOCK_EX | LOCK_NB) < 0) {
                const std::string message = "source camera '" + source
                    + "' is already locked by another process: "
                    + std::strerror(errno);
                lease.reset();
                throw std::runtime_error(message);
            }
#else
            static_cast<void>(source);
            static_cast<void>(require_capture_device);
#endif
            return lease;
        }

        [[nodiscard]] bool is_capture_device() const noexcept {
#ifdef __linux__
            return is_capture_device_;
#else
            return false;
#endif
        }

    private:
        // Release is an ownership mutation even though clang-tidy does not see
        // the Linux-only descriptor update in every analysis configuration.
        void
        reset() noexcept { // NOLINT(readability-make-member-function-const)
#ifdef __linux__
            if (fd_ >= 0) {
                ::flock(fd_, LOCK_UN);
                ::close(fd_);
                fd_ = -1;
            }
#endif
        }

#ifdef __linux__
        int fd_ { -1 };
        bool is_capture_device_ { false };
#endif
    };

    std::string daemon_state_text(const stream_daemon_state state) {
        switch (state) {
        case stream_daemon_state::starting:
            return "starting";
        case stream_daemon_state::running:
            return "running";
        case stream_daemon_state::stopping:
            return "stopping";
        case stream_daemon_state::completed:
            return "completed";
        case stream_daemon_state::failed:
            return "failed";
        }
        return "unknown";
    }

    bool
    same_device_identity(const std::string& source, const std::string& output) {
        if (source == output) {
            return true;
        }

        std::error_code equivalent_error;
        if (std::filesystem::equivalent(source, output, equivalent_error)
            && !equivalent_error) {
            return true;
        }

#ifdef __linux__
        struct stat source_stat {};
        struct stat output_stat {};
        if (::stat(source.c_str(), &source_stat) == 0
            && ::stat(output.c_str(), &output_stat) == 0
            && S_ISCHR(source_stat.st_mode) && S_ISCHR(output_stat.st_mode)) {
            return source_stat.st_rdev == output_stat.st_rdev;
        }
#endif
        return false;
    }

    bool is_linux_character_device(const std::string& path) {
#ifdef __linux__
        struct stat status {};
        return ::stat(path.c_str(), &status) == 0 && S_ISCHR(status.st_mode);
#else
        static_cast<void>(path);
        return false;
#endif
    }

} // namespace

std::string redact_capture_source_for_log(const std::string_view source) {
    const size_t scheme = source.find("://");
    if (scheme == std::string_view::npos) {
        return std::string(source);
    }
    // Credentials and camera access tokens are frequently embedded in URL
    // user-info, paths, queries, or fragments. Logging only the scheme avoids
    // trying to guess which URL components are sensitive.
    return std::string(source.substr(0U, scheme)) + "://<redacted>";
}

int run_headless_daemon(
    const headless_daemon_options& options, const stop_token& stop_token,
    std::ostream& output, std::ostream& errors
) {
    if (options.configuration_path.empty()) {
        errors << "a line configuration file is required\n";
        return 2;
    }

    try {
        const line_configuration_document document
            = load_line_configuration_file(options.configuration_path);
        monitor::record_breadcrumb(monitor::event::configuration_loaded);
        line_configuration_apply_options apply_options;
        apply_options.source_override = options.source_override;
        apply_options.virtual_camera_path_override
            = options.output_device_override;
        apply_options.stream_name_override = options.stream_name_override;

        const std::string source
            = options.source_override.value_or(document.stream.source);
        const std::string output_device
            = options.output_device_override.value_or(
                document.stream.virtual_camera_path
            );
        if (source.empty() || output_device.empty()) {
            throw std::runtime_error(
                "configuration must resolve both source and virtual camera "
                "paths"
            );
        }
        if (same_device_identity(source, output_device)) {
            throw std::runtime_error(
                "source camera and virtual camera must be different devices"
            );
        }

        const bool source_is_character_device
            = is_linux_character_device(source);
        const bool source_is_capture_device = is_linux_capture_device(source);
        const bool source_requires_local_type = source_is_character_device
            || source_is_capture_device
            || (!options.source_override.has_value()
                && document.stream.type == "local");
        if (source_requires_local_type) {
            // Never trust a configuration's declared type to downgrade a
            // device node into a file source. Doing so would bypass the
            // source lease and the post-start V4L2 exclusivity probe.
            apply_options.source_type_override = "local";
            apply_options.loop_override = false;
        } else if (options.source_override.has_value()) {
            const stream_type detected_type = stream::identify(source);
            apply_options.source_type_override
                = stream::type_name(detected_type);
            apply_options.loop_override = detected_type == stream_type::file
                ? document.stream.loop
                : false;
        }

        // flock rejects cooperating yodau processes immediately. After the
        // first processed frame, the V4L2 buffer-owner probe below verifies
        // driver-enforced exclusion and fails closed unless it observes EBUSY.
        const bool local_source
            = apply_options.source_type_override.value_or(document.stream.type)
            == "local";
        auto source_lease = source_camera_lease::acquire(source, local_source);
        static_cast<void>(source_lease);

        processing_runtime runtime(
            processing_runtime_options {
                .mode = render_mode::core_only,
                .enable_virtual_camera = true,
                .virtual_camera_device = output_device,
                .algorithm_id = document.stream.algorithm.algorithm_id,
            }
        );
        // Keep the manager after the runtime in construction order so it is
        // shut down first during exception unwinding. Its daemon callbacks may
        // still be executing code owned by the runtime.
        stream_manager manager;
        runtime.attach(manager);
        const line_configuration_apply_result applied
            = apply_line_configuration(
                document, manager, runtime, apply_options
            );
        const std::shared_ptr<const stream> configured_stream
            = manager.find_stream(applied.stream_name);
        if (!configured_stream) {
            throw std::runtime_error(
                "configuration did not create the requested stream"
            );
        }
        const bool persistent_source
            = configured_stream->get_type() != stream_type::file
            || configured_stream->is_looping();

        output << "starting headless stream '" << applied.stream_name
               << "' from " << redact_capture_source_for_log(applied.source)
               << " -> " << applied.virtual_camera_path << " with "
               << applied.connected_line_count << " configured lines\n";
        manager.start_stream(applied.stream_name);

        const auto initial_status = manager.stream_status(applied.stream_name);
        if (!initial_status.has_value()) {
            throw std::runtime_error(
                "stream could not start; check source permissions and "
                "algorithm support"
            );
        }
        monitor::record_breadcrumb(monitor::event::stream_started);

        bool output_ready = false;
        std::string output_error;
        const auto output_deadline
            = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!stop_token.stop_requested()
               && manager.is_stream_running(applied.stream_name)
               && std::chrono::steady_clock::now() < output_deadline) {
            monitor::heartbeat(monitor::channel::main);
            if (const virtual_camera* camera = runtime.preview_camera()) {
                const auto cameras = camera->frames();
                const auto info = std::ranges::find(
                    cameras, applied.stream_name,
                    &virtual_camera_frame_info::stream_name
                );
                if (info != cameras.end()) {
                    output_ready = info->device_ready;
                    output_error = info->last_error;
                }
            }
            if (output_ready) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (!stop_token.stop_requested() && !output_ready) {
            if (!manager.is_stream_running(applied.stream_name)) {
                const auto status = manager.stream_status(applied.stream_name);
                throw std::runtime_error(
                    "source capture ended before virtual camera startup"
                    + (status.has_value() && !status->error.empty()
                           ? ": " + status->error
                           : std::string {})
                );
            }
            manager.stop_stream(applied.stream_name);
            throw std::runtime_error(
                "virtual camera did not become ready at '"
                + applied.virtual_camera_path + "'"
                + (output_error.empty() ? std::string {} : ": " + output_error)
            );
        }
        monitor::record_breadcrumb(monitor::event::output_ready);

        if (!stop_token.stop_requested() && source_lease.is_capture_device()) {
            const linux_capture_exclusivity_result exclusivity
                = probe_linux_capture_exclusivity(applied.source);
            if (exclusivity.state != linux_capture_exclusivity::exclusive) {
                manager.stop_stream(applied.stream_name);
                throw std::runtime_error(
                    "source camera exclusivity could not be established: "
                    + exclusivity.detail
                );
            }
            output << "source exclusivity verified: " << exclusivity.detail
                   << '\n';
        }

        std::optional<std::chrono::steady_clock::time_point>
            output_unhealthy_since;
        std::string runtime_output_error;
        while (!stop_token.stop_requested()
               && manager.is_stream_running(applied.stream_name)) {
            monitor::heartbeat(monitor::channel::main);
            bool runtime_output_ready = false;
            if (const virtual_camera* camera = runtime.preview_camera()) {
                const auto cameras = camera->frames();
                const auto info = std::ranges::find(
                    cameras, applied.stream_name,
                    &virtual_camera_frame_info::stream_name
                );
                if (info != cameras.end()) {
                    runtime_output_ready = info->device_ready;
                    runtime_output_error = info->last_error;
                } else {
                    runtime_output_error = "virtual camera stream disappeared";
                }
            } else {
                runtime_output_error = "virtual camera publisher disappeared";
            }

            const auto now = std::chrono::steady_clock::now();
            if (runtime_output_ready) {
                output_unhealthy_since.reset();
                runtime_output_error.clear();
            } else if (!output_unhealthy_since.has_value()) {
                output_unhealthy_since = now;
            } else if (
                now - *output_unhealthy_since >= std::chrono::seconds(2)
            ) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        const auto final_status = manager.stream_status(applied.stream_name);
        const bool output_failed = !stop_token.stop_requested()
            && manager.is_stream_running(applied.stream_name)
            && output_unhealthy_since.has_value()
            && std::chrono::steady_clock::now() - *output_unhealthy_since
                >= std::chrono::seconds(2);
        manager.stop_stream(applied.stream_name);
        monitor::record_breadcrumb(monitor::event::stream_stopped);
        if (virtual_camera* camera = runtime.preview_camera()) {
            camera->release(applied.stream_name);
        }
        manager.shutdown();
        runtime.detach();

        if (output_failed) {
            errors << "virtual camera output failed";
            if (!runtime_output_error.empty()) {
                errors << ": " << runtime_output_error;
            }
            errors << '\n';
            return 1;
        }
        if (!stop_token.stop_requested() && final_status.has_value()) {
            errors << "stream " << daemon_state_text(final_status->state);
            if (!final_status->error.empty()) {
                errors << ": " << final_status->error;
            }
            errors << '\n';
            if (final_status->state != stream_daemon_state::completed) {
                return 1;
            }
            if (persistent_source) {
                errors << "persistent source ended unexpectedly\n";
                return 1;
            }
            return 0;
        }
        return 0;
    } catch (const line_configuration_error& error) {
        errors << "invalid line configuration: " << error.what() << '\n';
        return 2;
    } catch (const std::exception& error) {
        errors << "headless daemon failed: " << error.what() << '\n';
        return 1;
    }
}

} // namespace yodau::core
