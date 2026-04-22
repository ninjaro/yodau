#include "analysis/processing_runtime.hpp"

#include "analysis/default_processing_hooks.hpp"
#include "analysis/processing_algorithm_catalog.hpp"
#include "analysis/processing_preview_router.hpp"

namespace yodau::core {

namespace {

std::unique_ptr<processing_algorithm> make_configured_processing_algorithm(
    const processing_algorithm_settings& settings
) {
    const processing_algorithm_settings normalized_settings
        = normalized_processing_algorithm_settings(settings);
    if (normalized_settings.algorithm_id.empty()) {
        return {};
    }

    auto algorithm = make_processing_algorithm(normalized_settings.algorithm_id);
    if (!algorithm) {
        return {};
    }

    processing_algorithm_settings configured_settings = normalized_settings;
    configured_settings.algorithm_id = algorithm->algorithm_id();

    algorithm->configure(
        processing_algorithm_settings_configuration(configured_settings)
    );
    return algorithm;
}

std::unique_ptr<processing_algorithm> make_configured_processing_algorithm(
    const std::string& algorithm_id, const std::string& preset_id = {}
) {
    processing_algorithm_settings settings
        = default_processing_algorithm_settings(algorithm_id);
    if (!preset_id.empty()) {
        settings.preset_id = preset_id;
    }
    return make_configured_processing_algorithm(settings);
}

} // namespace

std::string render_mode_name(const render_mode mode) {
    switch (mode) {
    case render_mode::core_only:
        return "core_only";
    case render_mode::app_only:
        return "app_only";
    }

    return "app_only";
}

processing_runtime::processing_runtime(
    processing_runtime_options runtime_options_value
)
    : runtime_options(std::move(runtime_options_value))
    , session_store_(runtime_options.algorithm_id) {
    processing_algorithm_settings default_settings
        = default_processing_algorithm_settings(runtime_options.algorithm_id);
    if (auto default_algorithm
        = make_configured_processing_algorithm(default_settings)) {
        default_settings.algorithm_id = default_algorithm->algorithm_id();
        default_settings
            = normalized_processing_algorithm_settings(std::move(default_settings));
        runtime_options.algorithm_id = default_settings.algorithm_id;
        session_store_.set_default_algorithm(std::move(default_settings));
    } else {
        runtime_options.algorithm_id.clear();
        session_store_.set_default_algorithm(processing_algorithm_settings {});
    }

    if (runtime_options.mode == render_mode::core_only
        && runtime_options.enable_virtual_camera) {
        preview_router_value = std::make_unique<processing_preview_router>(true);
    }
}

processing_runtime::~processing_runtime() = default;

processing_runtime::processing_runtime(processing_runtime&& other) noexcept
    : runtime_options(other.runtime_options)
    , session_store_(std::move(other.session_store_))
    , preview_router_value(std::move(other.preview_router_value)) {}

processing_runtime&
processing_runtime::operator=(processing_runtime&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    runtime_options = other.runtime_options;
    session_store_ = std::move(other.session_store_);
    preview_router_value = std::move(other.preview_router_value);
    return *this;
}

void processing_runtime::attach(stream_manager& mgr) {
    mgr.set_frame_processor(frame_processor_hook());

    if (runtime_options.mode == render_mode::core_only) {
        mgr.set_daemon_start_hook(daemon_start_hook());
        mgr.set_processed_frame_sink(processed_frame_sink());
        return;
    }

    mgr.set_daemon_start_hook({});
    mgr.set_processed_frame_sink({});
}

stream_manager::daemon_start_fn processing_runtime::daemon_start_hook() {
    if (runtime_options.mode != render_mode::core_only
        || !processing_enabled()) {
        return {};
    }

    return std::bind_front(&processing_runtime::start_daemon, this);
}

stream_manager::frame_processor_fn processing_runtime::frame_processor_hook() {
    return processing_enabled()
        ? std::bind_front(&processing_runtime::process_frame, this)
        : stream_manager::frame_processor_fn {};
}

stream_manager::processed_frame_sink_fn
processing_runtime::processed_frame_sink() {
    if (runtime_options.mode != render_mode::core_only) {
        return {};
    }

    return std::bind_front(&processing_runtime::handle_processed_frame, this);
}

render_mode processing_runtime::mode() const { return runtime_options.mode; }

std::string processing_runtime::algorithm_id() const {
    return default_algorithm_id();
}

std::string processing_runtime::default_algorithm_id() const {
    return session_store_.default_algorithm_id();
}

processing_algorithm_settings processing_runtime::default_algorithm_settings() const {
    return session_store_.default_algorithm_settings();
}

std::string processing_runtime::algorithm_id_for_stream(
    const std::string& stream_name
) const {
    return session_store_.algorithm_id_for_stream(stream_name);
}

processing_algorithm_settings processing_runtime::algorithm_settings_for_stream(
    const std::string& stream_name
) const {
    return session_store_.algorithm_settings_for_stream(stream_name);
}

std::vector<std::string> processing_runtime::available_algorithm_ids() const {
    return default_processing_algorithm_registry().algorithm_ids();
}

std::unordered_map<std::string, std::string>
processing_runtime::stream_algorithm_overrides() const {
    return session_store_.stream_algorithm_overrides();
}

std::unordered_map<std::string, processing_algorithm_settings>
processing_runtime::stream_algorithm_setting_overrides() const {
    return session_store_.stream_algorithm_setting_overrides();
}

bool processing_runtime::set_default_algorithm(const std::string& algorithm_id) {
    return set_default_algorithm_settings(
        default_processing_algorithm_settings(algorithm_id)
    );
}

bool processing_runtime::set_default_algorithm_settings(
    processing_algorithm_settings settings
) {
    settings = normalized_processing_algorithm_settings(std::move(settings));
    auto algorithm = make_configured_processing_algorithm(settings);
    if (!algorithm) {
        return false;
    }

    const std::string canonical_algorithm_id = algorithm->algorithm_id();
    runtime_options.algorithm_id = canonical_algorithm_id;
    settings.algorithm_id = canonical_algorithm_id;
    settings = normalized_processing_algorithm_settings(std::move(settings));
    session_store_.set_default_algorithm(std::move(settings));
    return true;
}

bool processing_runtime::set_stream_algorithm(
    const std::string& stream_name, const std::string& algorithm_id,
    const std::string& preset_id
) {
    if (stream_name.empty()) {
        return false;
    }

    processing_algorithm_settings settings
        = default_processing_algorithm_settings(algorithm_id);
    if (!preset_id.empty()) {
        settings.preset_id = preset_id;
    }
    return set_stream_algorithm_settings(stream_name, std::move(settings));
}

bool processing_runtime::set_stream_algorithm_settings(
    const std::string& stream_name, processing_algorithm_settings settings
) {
    if (stream_name.empty()) {
        return false;
    }

    settings = normalized_processing_algorithm_settings(std::move(settings));
    auto algorithm = make_configured_processing_algorithm(settings);
    if (!algorithm) {
        return false;
    }

    const std::string canonical_algorithm_id = algorithm->algorithm_id();
    settings.algorithm_id = canonical_algorithm_id;
    settings = normalized_processing_algorithm_settings(std::move(settings));
    auto shared_algorithm
        = std::shared_ptr<processing_algorithm>(std::move(algorithm));
    session_store_.set_stream_algorithm(
        stream_name, std::move(settings), std::move(shared_algorithm)
    );
    return true;
}

bool processing_runtime::clear_stream_algorithm(const std::string& stream_name) {
    if (stream_name.empty()) {
        return false;
    }

    session_store_.clear_stream_algorithm(stream_name);
    return true;
}

bool processing_runtime::processing_enabled() const {
    return session_store_.processing_enabled();
}

bool processing_runtime::has_virtual_camera() const {
    return preview_router_value != nullptr
        && preview_router_value->has_virtual_camera();
}

void processing_runtime::set_processed_frame_observer(
    processed_frame_observer_fn observer
) {
    std::scoped_lock lock(observer_mtx);
    processed_frame_observer = std::move(observer);
}

std::optional<processing_result>
processing_runtime::latest_processing_result(
    const std::string& stream_name
) const {
    return session_store_.latest_processing_result(stream_name);
}

virtual_camera* processing_runtime::preview_camera() {
    return preview_router_value != nullptr ? preview_router_value->preview_camera()
                                           : nullptr;
}

const virtual_camera* processing_runtime::preview_camera() const {
    return preview_router_value != nullptr ? preview_router_value->preview_camera()
                                           : nullptr;
}

std::shared_ptr<processing_algorithm>
processing_runtime::active_algorithm_for_stream(const std::string& stream_name) {
    return session_store_.active_algorithm_for_stream(
        stream_name,
        [](const processing_algorithm_settings& settings) {
            return make_configured_processing_algorithm(settings);
        }
    );
}

void processing_runtime::start_daemon(
    const stream& stream_value, const std::function<void(frame&&)>& on_frame,
    const std::stop_token& stop_token
) {
    auto algorithm = active_algorithm_for_stream(stream_value.get_name());
    if (algorithm == nullptr) {
        return;
    }

    algorithm->daemon_start(stream_value, on_frame, stop_token);
}

std::vector<event>
processing_runtime::process_frame(const stream& s, const frame& f) {
    auto algorithm = active_algorithm_for_stream(s.get_name());
    if (algorithm == nullptr) {
        return {};
    }

    processing_result result = algorithm->process_frame(s, f);
    result = motion_region_filter_value.apply(s, std::move(result));
    session_store_.store_latest_processing_result(s.get_name(), result);

    processed_frame_observer_fn observer;
    {
        std::scoped_lock lock(observer_mtx);
        observer = processed_frame_observer;
    }
    if (observer) {
        observer(s, f, result);
    }

    return result.events;
}

void processing_runtime::handle_processed_frame(
    const stream& s, const frame& frame_value, const std::vector<event>& events
) {
    if (runtime_options.mode != render_mode::core_only
        || preview_router_value == nullptr) {
        return;
    }

    const auto latest_result = latest_processing_result(s.get_name());
    const processing_result* latest_result_ptr = latest_result.has_value()
        ? &latest_result.value()
        : nullptr;
    preview_router_value->publish_processed_frame(
        s, frame_value, events, latest_result_ptr
    );
}

} // namespace yodau::core
