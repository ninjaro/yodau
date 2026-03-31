#include "analysis/processing_runtime.hpp"

#include "analysis/default_processing_hooks.hpp"
#include "analysis/processing_preview_router.hpp"

namespace yodau::backend {

std::string render_mode_name(const render_mode mode) {
    switch (mode) {
    case render_mode::backend_only:
        return "backend_only";
    case render_mode::frontend_only:
        return "frontend_only";
    }

    return "frontend_only";
}

processing_runtime::processing_runtime(
    processing_runtime_options runtime_options_value
)
    : runtime_options(std::move(runtime_options_value)) {
    if (auto default_algorithm
        = make_processing_algorithm(runtime_options.algorithm_id)) {
        runtime_options.algorithm_id = default_algorithm->algorithm_id();
    } else {
        runtime_options.algorithm_id.clear();
    }

    if (runtime_options.mode == render_mode::backend_only
        && runtime_options.enable_virtual_camera) {
        preview_router_value = std::make_unique<processing_preview_router>(true);
    }
}

processing_runtime::~processing_runtime() = default;

processing_runtime::processing_runtime(processing_runtime&& other) noexcept
    : runtime_options(other.runtime_options)
    , preview_router_value(std::move(other.preview_router_value)) {
    std::scoped_lock lock(other.algorithms_mtx, other.latest_results_mtx);
    algorithm_overrides_by_stream = std::move(other.algorithm_overrides_by_stream);
    active_algorithms_by_stream = std::move(other.active_algorithms_by_stream);
    latest_results_by_stream = std::move(other.latest_results_by_stream);
}

processing_runtime&
processing_runtime::operator=(processing_runtime&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    std::scoped_lock lock(
        algorithms_mtx, latest_results_mtx, other.algorithms_mtx,
        other.latest_results_mtx
    );
    runtime_options = other.runtime_options;
    preview_router_value = std::move(other.preview_router_value);
    algorithm_overrides_by_stream = std::move(other.algorithm_overrides_by_stream);
    active_algorithms_by_stream = std::move(other.active_algorithms_by_stream);
    latest_results_by_stream = std::move(other.latest_results_by_stream);
    return *this;
}

void processing_runtime::attach(stream_manager& mgr) {
    mgr.set_frame_processor(frame_processor_hook());

    if (runtime_options.mode == render_mode::backend_only) {
        mgr.set_daemon_start_hook(daemon_start_hook());
        mgr.set_processed_frame_sink(processed_frame_sink());
        return;
    }

    mgr.set_daemon_start_hook({});
    mgr.set_processed_frame_sink({});
}

stream_manager::daemon_start_fn processing_runtime::daemon_start_hook() {
    if (runtime_options.mode != render_mode::backend_only
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
    if (runtime_options.mode != render_mode::backend_only) {
        return {};
    }

    return std::bind_front(&processing_runtime::handle_processed_frame, this);
}

render_mode processing_runtime::mode() const { return runtime_options.mode; }

std::string processing_runtime::algorithm_id() const {
    return default_algorithm_id();
}

std::string processing_runtime::default_algorithm_id() const {
    std::scoped_lock lock(algorithms_mtx);
    return runtime_options.algorithm_id;
}

std::string processing_runtime::algorithm_id_for_stream(
    const std::string& stream_name
) const {
    std::scoped_lock lock(algorithms_mtx);
    return resolved_algorithm_id_for_stream_locked(stream_name);
}

std::vector<std::string> processing_runtime::available_algorithm_ids() const {
    return default_processing_algorithm_registry().algorithm_ids();
}

std::unordered_map<std::string, std::string>
processing_runtime::stream_algorithm_overrides() const {
    std::scoped_lock lock(algorithms_mtx);
    return algorithm_overrides_by_stream;
}

bool processing_runtime::set_default_algorithm(const std::string& algorithm_id) {
    auto algorithm = make_processing_algorithm(algorithm_id);
    if (!algorithm) {
        return false;
    }

    const std::string canonical_algorithm_id = algorithm->algorithm_id();
    std::vector<std::string> streams_using_default;

    {
        std::scoped_lock lock(algorithms_mtx);
        runtime_options.algorithm_id = canonical_algorithm_id;

        for (auto it = active_algorithms_by_stream.begin();
             it != active_algorithms_by_stream.end();) {
            if (!algorithm_overrides_by_stream.contains(it->first)) {
                streams_using_default.push_back(it->first);
                it = active_algorithms_by_stream.erase(it);
                continue;
            }

            ++it;
        }
    }

    if (!streams_using_default.empty()) {
        std::scoped_lock lock(latest_results_mtx);
        for (const std::string& stream_name : streams_using_default) {
            latest_results_by_stream.erase(stream_name);
        }
    }

    return true;
}

bool processing_runtime::set_stream_algorithm(
    const std::string& stream_name, const std::string& algorithm_id
) {
    if (stream_name.empty()) {
        return false;
    }

    auto algorithm = make_processing_algorithm(algorithm_id);
    if (!algorithm) {
        return false;
    }

    const std::string canonical_algorithm_id = algorithm->algorithm_id();
    auto shared_algorithm
        = std::shared_ptr<processing_algorithm>(std::move(algorithm));

    {
        std::scoped_lock lock(algorithms_mtx);
        if (canonical_algorithm_id == runtime_options.algorithm_id) {
            algorithm_overrides_by_stream.erase(stream_name);
        } else {
            algorithm_overrides_by_stream[stream_name] = canonical_algorithm_id;
        }

        active_algorithms_by_stream[stream_name] = std::move(shared_algorithm);
    }

    clear_latest_processing_result(stream_name);
    return true;
}

bool processing_runtime::clear_stream_algorithm(const std::string& stream_name) {
    if (stream_name.empty()) {
        return false;
    }

    {
        std::scoped_lock lock(algorithms_mtx);
        algorithm_overrides_by_stream.erase(stream_name);
        active_algorithms_by_stream.erase(stream_name);
    }

    clear_latest_processing_result(stream_name);
    return true;
}

bool processing_runtime::processing_enabled() const {
    return !default_algorithm_id().empty();
}

bool processing_runtime::has_virtual_camera() const {
    return preview_router_value != nullptr
        && preview_router_value->has_virtual_camera();
}

std::optional<processing_result>
processing_runtime::latest_processing_result(
    const std::string& stream_name
) const {
    std::scoped_lock lock(latest_results_mtx);
    const auto it = latest_results_by_stream.find(stream_name);
    return it == latest_results_by_stream.end() ? std::nullopt
                                                : std::optional<processing_result>(it->second);
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
    if (stream_name.empty()) {
        return {};
    }

    std::scoped_lock lock(algorithms_mtx);
    const std::string resolved_algorithm_id
        = resolved_algorithm_id_for_stream_locked(stream_name);
    if (resolved_algorithm_id.empty()) {
        return {};
    }

    const auto algorithm_it = active_algorithms_by_stream.find(stream_name);
    if (algorithm_it != active_algorithms_by_stream.end()
        && algorithm_it->second != nullptr
        && algorithm_it->second->algorithm_id() == resolved_algorithm_id) {
        return algorithm_it->second;
    }

    auto algorithm = make_processing_algorithm(resolved_algorithm_id);
    if (!algorithm) {
        return {};
    }

    auto shared_algorithm
        = std::shared_ptr<processing_algorithm>(std::move(algorithm));
    active_algorithms_by_stream[stream_name] = shared_algorithm;
    return shared_algorithm;
}

std::string processing_runtime::resolved_algorithm_id_for_stream_locked(
    const std::string& stream_name
) const {
    const auto override_it = algorithm_overrides_by_stream.find(stream_name);
    if (override_it != algorithm_overrides_by_stream.end()) {
        return override_it->second;
    }

    return runtime_options.algorithm_id;
}

void processing_runtime::clear_latest_processing_result(
    const std::string& stream_name
) {
    std::scoped_lock lock(latest_results_mtx);
    latest_results_by_stream.erase(stream_name);
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
    {
        std::scoped_lock lock(latest_results_mtx);
        latest_results_by_stream[s.get_name()] = result;
    }
    return result.events;
}

void processing_runtime::handle_processed_frame(
    const stream& s, const frame& frame_value, const std::vector<event>& events
) {
    if (runtime_options.mode != render_mode::backend_only
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

}
