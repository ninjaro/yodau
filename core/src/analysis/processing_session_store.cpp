#include "analysis/processing_session_store.hpp"

#include <utility>
#include <vector>

namespace yodau::core {

processing_session_store::processing_session_store(
    std::string default_algorithm_id
)
    : default_algorithm_id_(std::move(default_algorithm_id)) {}

processing_session_store::processing_session_store(
    processing_session_store&& other
) noexcept {
    std::scoped_lock lock(other.algorithms_mtx_, other.latest_results_mtx_);
    default_algorithm_id_ = std::move(other.default_algorithm_id_);
    algorithm_overrides_by_stream_
        = std::move(other.algorithm_overrides_by_stream_);
    active_algorithms_by_stream_ = std::move(other.active_algorithms_by_stream_);
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
    default_algorithm_id_ = std::move(other.default_algorithm_id_);
    algorithm_overrides_by_stream_
        = std::move(other.algorithm_overrides_by_stream_);
    active_algorithms_by_stream_ = std::move(other.active_algorithms_by_stream_);
    latest_results_by_stream_ = std::move(other.latest_results_by_stream_);
    return *this;
}

std::string processing_session_store::default_algorithm_id() const {
    std::scoped_lock lock(algorithms_mtx_);
    return default_algorithm_id_;
}

std::string processing_session_store::algorithm_id_for_stream(
    const std::string& stream_name
) const {
    std::scoped_lock lock(algorithms_mtx_);
    return resolved_algorithm_id_for_stream_locked(stream_name);
}

std::unordered_map<std::string, std::string>
processing_session_store::stream_algorithm_overrides() const {
    std::scoped_lock lock(algorithms_mtx_);
    return algorithm_overrides_by_stream_;
}

bool processing_session_store::processing_enabled() const {
    return !default_algorithm_id().empty();
}

void processing_session_store::set_default_algorithm(
    std::string canonical_algorithm_id
) {
    std::vector<std::string> streams_using_default;

    {
        std::scoped_lock lock(algorithms_mtx_);
        default_algorithm_id_ = std::move(canonical_algorithm_id);

        for (auto it = active_algorithms_by_stream_.begin();
             it != active_algorithms_by_stream_.end();) {
            if (!algorithm_overrides_by_stream_.contains(it->first)) {
                streams_using_default.push_back(it->first);
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

void processing_session_store::set_stream_algorithm(
    const std::string& stream_name, std::string canonical_algorithm_id,
    std::shared_ptr<processing_algorithm> algorithm
) {
    if (stream_name.empty()) {
        return;
    }

    {
        std::scoped_lock lock(algorithms_mtx_);
        if (canonical_algorithm_id == default_algorithm_id_) {
            algorithm_overrides_by_stream_.erase(stream_name);
        } else {
            algorithm_overrides_by_stream_[stream_name]
                = std::move(canonical_algorithm_id);
        }

        active_algorithms_by_stream_[stream_name] = std::move(algorithm);
    }

    clear_latest_processing_result(stream_name);
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
    const std::string resolved_algorithm_id
        = resolved_algorithm_id_for_stream_locked(stream_name);
    if (resolved_algorithm_id.empty()) {
        return {};
    }

    const auto algorithm_it = active_algorithms_by_stream_.find(stream_name);
    if (algorithm_it != active_algorithms_by_stream_.end()
        && algorithm_it->second != nullptr
        && algorithm_it->second->algorithm_id() == resolved_algorithm_id) {
        return algorithm_it->second;
    }

    if (!factory) {
        return {};
    }

    auto algorithm = factory(resolved_algorithm_id);
    if (!algorithm) {
        return {};
    }

    auto shared_algorithm
        = std::shared_ptr<processing_algorithm>(std::move(algorithm));
    active_algorithms_by_stream_[stream_name] = shared_algorithm;
    return shared_algorithm;
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

std::string processing_session_store::resolved_algorithm_id_for_stream_locked(
    const std::string& stream_name
) const {
    const auto override_it = algorithm_overrides_by_stream_.find(stream_name);
    if (override_it != algorithm_overrides_by_stream_.end()) {
        return override_it->second;
    }

    return default_algorithm_id_;
}

} // namespace yodau::core
