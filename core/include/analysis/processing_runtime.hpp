#ifndef YODAU_CORE_ANALYSIS_PROCESSING_RUNTIME_HPP
#define YODAU_CORE_ANALYSIS_PROCESSING_RUNTIME_HPP

#include "analysis/processing_algorithm.hpp"
#include "analysis/processing_session_store.hpp"
#include "streams/stream_manager.hpp"

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
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
    // Optional explicit V4L2 output device (for example /dev/yodau0).
    std::string virtual_camera_device;
    // Default algorithm used for streams without an explicit override.
    std::string algorithm_id;
};

std::string render_mode_name(render_mode mode);

class processing_runtime {
public:
    using processed_frame_observer_fn = std::function<void(
        const stream& s, const frame& frame_value,
        const processing_result& result
    )>;

    explicit processing_runtime(
        processing_runtime_options runtime_options = {}
    );
    ~processing_runtime();

    processing_runtime(const processing_runtime&) = delete;
    processing_runtime& operator=(const processing_runtime&) = delete;
    processing_runtime(processing_runtime&&) = delete;
    processing_runtime& operator=(processing_runtime&&) = delete;

    void attach(stream_manager& mgr);
    void detach();

    stream_manager::daemon_start_fn daemon_start_hook();
    stream_manager::frame_processor_fn frame_processor_hook();
    stream_manager::processed_frame_sink_fn processed_frame_sink();
    stream_manager::stream_removed_sink_fn stream_removed_sink();

    render_mode mode() const;
    std::string algorithm_id() const;
    std::string default_algorithm_id() const;
    processing_algorithm_settings default_algorithm_settings() const;
    std::string algorithm_id_for_stream(const std::string& stream_name) const;
    processing_algorithm_settings
    algorithm_settings_for_stream(const std::string& stream_name) const;
    static std::vector<std::string> available_algorithm_ids();
    std::unordered_map<std::string, std::string>
    stream_algorithm_overrides() const;
    std::unordered_map<std::string, processing_algorithm_settings>
    stream_algorithm_setting_overrides() const;
    bool set_default_algorithm(const std::string& algorithm_id);
    bool set_default_algorithm_settings(processing_algorithm_settings settings);
    bool set_stream_algorithm(
        const std::string& stream_name, const std::string& algorithm_id,
        const std::string& preset_id = {}
    );
    bool set_stream_algorithm_settings(
        const std::string& stream_name, processing_algorithm_settings settings
    );
    [[nodiscard]] static bool
    supports_algorithm_settings(const processing_algorithm_settings& settings);
    bool clear_stream_algorithm(const std::string& stream_name);
    void set_stream_processing_max_pixels(
        const std::string& stream_name, std::optional<int> max_pixels
    );
    [[nodiscard]] std::optional<int>
    stream_processing_max_pixels(const std::string& stream_name) const;
    bool processing_enabled() const;
    bool has_virtual_camera() const;
    void set_processed_frame_observer(processed_frame_observer_fn observer);
    std::optional<processing_result>
    latest_processing_result(const std::string& stream_name) const;
    virtual_camera* preview_camera();
    const virtual_camera* preview_camera() const;

private:
    struct callback_state {
        std::mutex mtx;
        std::condition_variable idle;
        processing_runtime* owner { nullptr };
        std::size_t active_callbacks { 0U };
    };

    class callback_guard {
    public:
        explicit callback_guard(std::shared_ptr<callback_state> state);
        ~callback_guard();

        callback_guard(const callback_guard&) = delete;
        callback_guard& operator=(const callback_guard&) = delete;

        [[nodiscard]] processing_runtime* owner() const noexcept;

    private:
        std::shared_ptr<callback_state> state_;
        processing_runtime* owner_ { nullptr };
    };

    void invalidate_callbacks();
    std::vector<event> process_frame(const stream& s, const frame& f);
    void handle_processed_frame(
        const stream& s, const frame& frame_value,
        const std::vector<event>& events
    );
    void release_stream_state(const std::string& stream_name);
    std::shared_ptr<processing_algorithm>
    active_algorithm_for_stream(const std::string& stream_name);

    processing_runtime_options runtime_options;
    processing_session_store session_store_;
    std::unique_ptr<processing_preview_router> preview_router_value;
    std::shared_ptr<callback_state> callback_state_;
    mutable std::mutex observer_mtx;
    processed_frame_observer_fn processed_frame_observer;
    mutable std::mutex processing_policy_mtx;
    std::unordered_map<std::string, int> processing_max_pixels_by_stream;
};

}

#endif // YODAU_CORE_ANALYSIS_PROCESSING_RUNTIME_HPP
