#ifndef YODAU_CORE_ANALYSIS_PROCESSING_SESSION_STORE_HPP
#define YODAU_CORE_ANALYSIS_PROCESSING_SESSION_STORE_HPP

#include "core/namespace_alias.hpp"
#include "analysis/processing_algorithm.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace yodau::core {

class processing_session_store {
public:
    using algorithm_factory = std::function<std::unique_ptr<processing_algorithm>(
        const std::string& algorithm_id
    )>;

    explicit processing_session_store(std::string default_algorithm_id = {});
    ~processing_session_store() = default;

    processing_session_store(const processing_session_store&) = delete;
    processing_session_store& operator=(const processing_session_store&) = delete;
    processing_session_store(processing_session_store&& other) noexcept;
    processing_session_store&
    operator=(processing_session_store&& other) noexcept;

    std::string default_algorithm_id() const;
    std::string algorithm_id_for_stream(const std::string& stream_name) const;
    std::unordered_map<std::string, std::string> stream_algorithm_overrides()
        const;
    bool processing_enabled() const;

    void set_default_algorithm(std::string canonical_algorithm_id);
    void set_stream_algorithm(
        const std::string& stream_name, std::string canonical_algorithm_id,
        std::shared_ptr<processing_algorithm> algorithm
    );
    void clear_stream_algorithm(const std::string& stream_name);

    std::shared_ptr<processing_algorithm> active_algorithm_for_stream(
        const std::string& stream_name, const algorithm_factory& factory
    );

    std::optional<processing_result>
    latest_processing_result(const std::string& stream_name) const;
    void store_latest_processing_result(
        const std::string& stream_name, processing_result result
    );
    void clear_latest_processing_result(const std::string& stream_name);

private:
    std::string
    resolved_algorithm_id_for_stream_locked(const std::string& stream_name) const;

    mutable std::mutex algorithms_mtx_;
    std::string default_algorithm_id_;
    std::unordered_map<std::string, std::string> algorithm_overrides_by_stream_;
    std::unordered_map<std::string, std::shared_ptr<processing_algorithm>>
        active_algorithms_by_stream_;

    mutable std::mutex latest_results_mtx_;
    std::unordered_map<std::string, processing_result> latest_results_by_stream_;
};

} // namespace yodau::core

#endif // YODAU_CORE_ANALYSIS_PROCESSING_SESSION_STORE_HPP
