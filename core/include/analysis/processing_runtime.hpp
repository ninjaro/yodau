#ifndef YODAU_CORE_ANALYSIS_PROCESSING_RUNTIME_HPP
#define YODAU_CORE_ANALYSIS_PROCESSING_RUNTIME_HPP

#include "core/namespace_alias.hpp"
#include "analysis/processing_algorithm.hpp"
#include "analysis/processing_motion_region_filter.hpp"
#include "analysis/processing_session_store.hpp"
#include "streams/stream_manager.hpp"

#include <functional>
#include <mutex>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace yodau::core {

class processing_preview_router;
class virtual_camera;

enum class render_mode {
    core_only,
    app_only,
    backend_only = core_only,
    frontend_only = app_only,
};

struct processing_runtime_options {
    render_mode mode { render_mode::app_only };
    bool enable_virtual_camera { false };
    // Default algorithm used for streams without an explicit override.
    std::string algorithm_id;
};

std::string render_mode_name(render_mode mode);

class processing_runtime {
public:
    using processed_frame_observer_fn = std::function<void(
        const stream& s, const frame& frame_value, const processing_result& result
    )>;

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
    std::string algorithm_id() const;
    std::string default_algorithm_id() const;
    std::string algorithm_id_for_stream(const std::string& stream_name) const;
    std::vector<std::string> available_algorithm_ids() const;
    std::unordered_map<std::string, std::string> stream_algorithm_overrides()
        const;
    bool set_default_algorithm(const std::string& algorithm_id);
    bool set_stream_algorithm(
        const std::string& stream_name, const std::string& algorithm_id
    );
    bool clear_stream_algorithm(const std::string& stream_name);
    bool processing_enabled() const;
    bool has_virtual_camera() const;
    void set_processed_frame_observer(processed_frame_observer_fn observer);
    std::optional<processing_result>
    latest_processing_result(const std::string& stream_name) const;
    virtual_camera* preview_camera();
    const virtual_camera* preview_camera() const;

private:
    void start_daemon(
        const stream& stream_value, const std::function<void(frame&&)>& on_frame,
        const std::stop_token& stop_token
    );
    std::vector<event> process_frame(const stream& s, const frame& f);
    void handle_processed_frame(
        const stream& s, const frame& frame_value,
        const std::vector<event>& events
    );
    std::shared_ptr<processing_algorithm>
    active_algorithm_for_stream(const std::string& stream_name);

    processing_runtime_options runtime_options;
    processing_motion_region_filter motion_region_filter_value;
    processing_session_store session_store_;
    std::unique_ptr<processing_preview_router> preview_router_value;
    mutable std::mutex observer_mtx;
    processed_frame_observer_fn processed_frame_observer;
};

}

#endif // YODAU_CORE_ANALYSIS_PROCESSING_RUNTIME_HPP
