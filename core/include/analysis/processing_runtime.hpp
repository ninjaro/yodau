#ifndef YODAU_BACKEND_ANALYSIS_PROCESSING_RUNTIME_HPP
#define YODAU_BACKEND_ANALYSIS_PROCESSING_RUNTIME_HPP

#include "analysis/processing_algorithm.hpp"
#include "streams/stream_manager.hpp"

#include <mutex>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace yodau::backend {

class processing_preview_router;
class virtual_camera;

enum class render_mode { backend_only, frontend_only };

struct processing_runtime_options {
    render_mode mode { render_mode::frontend_only };
    bool enable_virtual_camera { false };
    // Default algorithm used for streams without an explicit override.
    std::string algorithm_id;
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
    std::string
    resolved_algorithm_id_for_stream_locked(const std::string& stream_name) const;
    void clear_latest_processing_result(const std::string& stream_name);

    processing_runtime_options runtime_options;
    std::unique_ptr<processing_preview_router> preview_router_value;
    mutable std::mutex algorithms_mtx;
    std::unordered_map<std::string, std::string> algorithm_overrides_by_stream;
    std::unordered_map<std::string, std::shared_ptr<processing_algorithm>>
        active_algorithms_by_stream;
    mutable std::mutex latest_results_mtx;
    std::unordered_map<std::string, processing_result> latest_results_by_stream;
};

}

#endif // YODAU_BACKEND_ANALYSIS_PROCESSING_RUNTIME_HPP
