#ifndef YODAU_BACKEND_ANALYSIS_PROCESSING_RUNTIME_HPP
#define YODAU_BACKEND_ANALYSIS_PROCESSING_RUNTIME_HPP

#include "streams/stream_manager.hpp"

#include <memory>
#include <string>

namespace yodau::backend {

class virtual_camera;

enum class render_mode { backend_only, frontend_only };

struct processing_runtime_options {
    render_mode mode { render_mode::frontend_only };
    bool enable_virtual_camera { false };
};

std::string render_mode_name(render_mode mode);

class processing_runtime {
public:
    explicit processing_runtime(
        processing_runtime_options runtime_options = {}
    );
    ~processing_runtime();

    processing_runtime(const processing_runtime&) = delete;
    processing_runtime& operator=(const processing_runtime&) = delete;
    processing_runtime(processing_runtime&&) noexcept;
    processing_runtime& operator=(processing_runtime&&) noexcept;

    void attach(stream_manager& mgr);

    stream_manager::daemon_start_fn daemon_start_hook();
    stream_manager::frame_processor_fn frame_processor_hook();
    stream_manager::processed_frame_sink_fn processed_frame_sink();

    render_mode mode() const;
    bool processing_enabled() const;
    bool has_virtual_camera() const;
    virtual_camera* preview_camera();
    const virtual_camera* preview_camera() const;

private:
    std::vector<event> process_frame(const stream& s, const frame& f);
    void handle_processed_frame(
        const stream& s, const frame& frame_value,
        const std::vector<event>& events
    );

    processing_runtime_options runtime_options;
    std::unique_ptr<virtual_camera> preview_camera_value;
};

}

#endif // YODAU_BACKEND_ANALYSIS_PROCESSING_RUNTIME_HPP
