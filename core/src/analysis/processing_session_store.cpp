#include "analysis/processing_session_store.hpp"

#include <utility>
#include <vector>

namespace yodau::core {

processing_session_store::processing_session_store(
    const std::string& default_algorithm_id
)
    : default_algorithm_settings_(
          default_processing_algorithm_settings(default_algorithm_id)
      ) { }

processing_session_store::processing_session_store(
    processing_session_store&& other
) noexcept {
    std::scoped_lock lock(other.algorithms_mtx_, other.latest_results_mtx_);
    default_algorithm_settings_ = std::move(other.default_algorithm_settings_);
    algorithm_overrides_by_stream_
        = std::move(other.algorithm_overrides_by_stream_);
    active_algorithms_by_stream_
        = std::move(other.active_algorithms_by_stream_);
    active_settings_by_stream_ = std::move(other.active_settings_by_stream_);
    latest_results_by_stream_ = std::move(other.latest_results_by_stream_);
}

processing_session_store&
processing_session_store::operator=(processing_session_store&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    std::scoped_lock lock(
        algorithms_mtx_, latest_results_mtx_, other.algorithms_mtx_,
        other.latest_results_mtx_
    );
    default_algorithm_settings_ = std::move(other.default_algorithm_settings_);
    algorithm_overrides_by_stream_
        = std::move(other.algorithm_overrides_by_stream_);
    active_algorithms_by_stream_
        = std::move(other.active_algorithms_by_stream_);
    active_settings_by_stream_ = std::move(other.active_settings_by_stream_);
    latest_results_by_stream_ = std::move(other.latest_results_by_stream_);
    return *this;
}

std::string processing_session_store::default_algorithm_id() const {
    std::scoped_lock lock(algorithms_mtx_);
    return default_algorithm_settings_.algorithm_id;
}

processing_algorithm_settings
processing_session_store::default_algorithm_settings() const {
    std::scoped_lock lock(algorithms_mtx_);
    return default_algorithm_settings_;
}

std::string processing_session_store::algorithm_id_for_stream(
    const std::string& stream_name
) const {
    std::scoped_lock lock(algorithms_mtx_);
    return resolved_settings_for_stream_locked(stream_name).algorithm_id;
}

processing_algorithm_settings
processing_session_store::algorithm_settings_for_stream(
    const std::string& stream_name
) const {
    std::scoped_lock lock(algorithms_mtx_);
    return resolved_settings_for_stream_locked(stream_name);
}

std::unordered_map<std::string, std::string>
processing_session_store::stream_algorithm_overrides() const {
    std::scoped_lock lock(algorithms_mtx_);
    std::unordered_map<std::string, std::string> overrides;
    overrides.reserve(algorithm_overrides_by_stream_.size());
    for (const auto& [stream_name, settings] : algorithm_overrides_by_stream_) {
        overrides.emplace(stream_name, settings.algorithm_id);
    }
    return overrides;
}

std::unordered_map<std::string, processing_algorithm_settings>
processing_session_store::stream_algorithm_setting_overrides() const {
    std::scoped_lock lock(algorithms_mtx_);
    return algorithm_overrides_by_stream_;
}

bool processing_session_store::processing_enabled() const {
    return !default_algorithm_id().empty();
}

void processing_session_store::set_default_algorithm(
    processing_algorithm_settings settings
) {
    std::vector<std::string> streams_using_default;
    settings = normalized_processing_algorithm_settings(std::move(settings));

    {
        std::scoped_lock lock(algorithms_mtx_);
        default_algorithm_settings_ = std::move(settings);

        for (auto it = active_algorithms_by_stream_.begin();
             it != active_algorithms_by_stream_.end();) {
            if (!algorithm_overrides_by_stream_.contains(it->first)) {
                streams_using_default.push_back(it->first);
                active_settings_by_stream_.erase(it->first);
                it = active_algorithms_by_stream_.erase(it);
                continue;
            }

            ++it;
        }
    }

    if (streams_using_default.empty()) {
        return;
    }

    std::scoped_lock lock(latest_results_mtx_);
    for (const std::string& stream_name : streams_using_default) {
        latest_results_by_stream_.erase(stream_name);
    }
}

void processing_session_store::set_default_algorithm(
    const std::string& canonical_algorithm_id
) {
    set_default_algorithm(
        default_processing_algorithm_settings(canonical_algorithm_id)
    );
}

void processing_session_store::set_stream_algorithm(
    const std::string& stream_name, processing_algorithm_settings settings,
    std::shared_ptr<processing_algorithm> algorithm
) {
    if (stream_name.empty()) {
        return;
    }

    settings = normalized_processing_algorithm_settings(std::move(settings));

    {
        std::scoped_lock lock(algorithms_mtx_);
        if (settings == default_algorithm_settings_) {
            algorithm_overrides_by_stream_.erase(stream_name);
        } else {
            algorithm_overrides_by_stream_[stream_name] = settings;
        }

        active_settings_by_stream_[stream_name] = std::move(settings);
        active_algorithms_by_stream_[stream_name] = std::move(algorithm);
    }

    clear_latest_processing_result(stream_name);
}

void processing_session_store::set_stream_algorithm(
    const std::string& stream_name, const std::string& canonical_algorithm_id,
    std::shared_ptr<processing_algorithm> algorithm
) {
    set_stream_algorithm(
        stream_name,
        default_processing_algorithm_settings(canonical_algorithm_id),
        std::move(algorithm)
    );
}

void processing_session_store::clear_stream_algorithm(
    const std::string& stream_name
) {
    if (stream_name.empty()) {
        return;
    }

    {
        std::scoped_lock lock(algorithms_mtx_);
        algorithm_overrides_by_stream_.erase(stream_name);
        active_algorithms_by_stream_.erase(stream_name);
        active_settings_by_stream_.erase(stream_name);
    }

    clear_latest_processing_result(stream_name);
}

std::shared_ptr<processing_algorithm>
processing_session_store::active_algorithm_for_stream(
    const std::string& stream_name, const algorithm_factory& factory
) {
    if (stream_name.empty()) {
        return {};
    }

    std::scoped_lock lock(algorithms_mtx_);
    const processing_algorithm_settings resolved_settings
        = resolved_settings_for_stream_locked(stream_name);
    if (resolved_settings.algorithm_id.empty()) {
        return {};
    }

    const auto algorithm_it = active_algorithms_by_stream_.find(stream_name);
    const auto settings_it = active_settings_by_stream_.find(stream_name);
    if (algorithm_it != active_algorithms_by_stream_.end()
        && algorithm_it->second != nullptr
        && settings_it != active_settings_by_stream_.end()
        && settings_it->second == resolved_settings) {
        return algorithm_it->second;
    }

    if (!factory) {
        return {};
    }

    auto algorithm = factory(resolved_settings);
    if (!algorithm) {
        return {};
    }

    auto shared_algorithm
        = std::shared_ptr<processing_algorithm>(std::move(algorithm));
    active_algorithms_by_stream_[stream_name] = shared_algorithm;
    active_settings_by_stream_[stream_name] = resolved_settings;
    return shared_algorithm;
}

std::shared_ptr<processing_algorithm>
processing_session_store::active_algorithm_for_stream(
    const std::string& stream_name, const legacy_algorithm_factory& factory
) {
    if (!factory) {
        return {};
    }

    return active_algorithm_for_stream(
        stream_name, [&factory](const processing_algorithm_settings& settings) {
            return factory(settings.algorithm_id);
        }
    );
}

std::optional<processing_result>
processing_session_store::latest_processing_result(
    const std::string& stream_name
) const {
    std::scoped_lock lock(latest_results_mtx_);
    const auto it = latest_results_by_stream_.find(stream_name);
    return it == latest_results_by_stream_.end()
        ? std::nullopt
        : std::optional<processing_result>(it->second);
}

void processing_session_store::store_latest_processing_result(
    const std::string& stream_name, processing_result result
) {
    if (stream_name.empty()) {
        return;
    }

    std::scoped_lock lock(latest_results_mtx_);
    latest_results_by_stream_[stream_name] = std::move(result);
}

void processing_session_store::clear_latest_processing_result(
    const std::string& stream_name
) {
    std::scoped_lock lock(latest_results_mtx_);
    latest_results_by_stream_.erase(stream_name);
}

processing_algorithm_settings
processing_session_store::resolved_settings_for_stream_locked(
    const std::string& stream_name
) const {
    const auto override_it = algorithm_overrides_by_stream_.find(stream_name);
    if (override_it != algorithm_overrides_by_stream_.end()) {
        return override_it->second;
    }

    return default_algorithm_settings_;
}

} // namespace yodau::core
