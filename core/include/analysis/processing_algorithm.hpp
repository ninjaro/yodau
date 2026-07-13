#ifndef YODAU_CORE_ANALYSIS_PROCESSING_ALGORITHM_HPP
#define YODAU_CORE_ANALYSIS_PROCESSING_ALGORITHM_HPP

#include "core/namespace_alias.hpp"
#include "streams/event.hpp"
#include "streams/frame.hpp"
#include "streams/stream.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace yodau::core {

struct processing_metric {
    std::string name;
    double value { 0.0 };
    std::string unit;
};

struct processing_diagnostic {
    std::string key;
    std::string value;
};

enum class processing_overlay_kind { point, polyline, polygon, label };

struct processing_overlay {
    processing_overlay_kind kind { processing_overlay_kind::point };
    std::string label;
    std::vector<point> points_pct;
    std::optional<point> anchor_pct;
};

struct processing_algorithm_configuration {
    std::unordered_map<std::string, std::string> values;
};

struct processing_result {
    std::vector<event> events;
    std::vector<processing_overlay> overlays;
    std::vector<processing_metric> metrics;
    std::vector<processing_diagnostic> diagnostics;
};

class processing_algorithm {
public:
    virtual ~processing_algorithm() = default;

    [[nodiscard]] virtual std::string algorithm_id() const = 0;
    [[nodiscard]] virtual std::string display_name() const = 0;

    [[nodiscard]] virtual processing_algorithm_configuration
    default_configuration() const {
        return {};
    }

    [[nodiscard]] virtual processing_algorithm_configuration
    configuration() const {
        return default_configuration();
    }

    // The by-value API lets stateful overrides move configuration into storage.
    virtual void configure(
        processing_algorithm_configuration
            configuration // NOLINT(performance-unnecessary-value-param)
    ) {
        (void)configuration;
    }

    virtual void daemon_start(
        const stream& stream_value,
        const std::function<void(frame&&)>& on_frame,
        const std::stop_token& stop_token
    ) = 0;

    virtual processing_result
    process_frame(const stream& stream_value, const frame& frame_value) = 0;
};

inline processing_metric make_processing_metric(
    std::string name, const double value, std::string unit = {}
) {
    return processing_metric {
        .name = std::move(name),
        .value = value,
        .unit = std::move(unit),
    };
}

inline void add_processing_metric(
    processing_result& result, std::string name, const double value,
    std::string unit = {}
) {
    result.metrics.push_back(
        make_processing_metric(std::move(name), value, std::move(unit))
    );
}

inline processing_diagnostic
make_processing_diagnostic(std::string key, std::string value) {
    return processing_diagnostic {
        .key = std::move(key),
        .value = std::move(value),
    };
}

inline void add_processing_diagnostic(
    processing_result& result, std::string key, std::string value
) {
    result.diagnostics.push_back(
        make_processing_diagnostic(std::move(key), std::move(value))
    );
}

inline void add_algorithm_diagnostics(
    processing_result& result, const processing_algorithm& algorithm
) {
    add_processing_diagnostic(result, "algorithm", algorithm.algorithm_id());
    add_processing_diagnostic(result, "display_name", algorithm.display_name());
}

inline void add_prefixed_processing_metrics(
    processing_result& result, std::vector<processing_metric> metrics,
    const std::string_view prefix
) {
    for (auto& metric : metrics) {
        metric.name = std::string(prefix) + metric.name;
        result.metrics.push_back(std::move(metric));
    }
}

class processing_algorithm_registry {
public:
    struct entry {
        std::string algorithm_id;
        std::string display_name;
        std::function<std::unique_ptr<processing_algorithm>()> create;
    };

    bool register_algorithm(entry entry_value) {
        entry_value.algorithm_id
            = normalized_algorithm_id(entry_value.algorithm_id);
        if (entry_value.algorithm_id.empty() || !entry_value.create) {
            return false;
        }
        if (entries_.contains(entry_value.algorithm_id)) {
            return false;
        }

        entries_.emplace(entry_value.algorithm_id, std::move(entry_value));
        return true;
    }

    [[nodiscard]] bool contains(const std::string& algorithm_id) const {
        return entries_.contains(normalized_algorithm_id(algorithm_id));
    }

    [[nodiscard]] std::optional<entry>
    find(const std::string& algorithm_id) const {
        const auto it = entries_.find(normalized_algorithm_id(algorithm_id));
        return it == entries_.end() ? std::nullopt
                                    : std::optional<entry>(it->second);
    }

    [[nodiscard]] std::unique_ptr<processing_algorithm>
    create(const std::string& algorithm_id) const {
        const auto entry_value = find(algorithm_id);
        return entry_value.has_value() ? entry_value->create() : nullptr;
    }

    [[nodiscard]] std::vector<std::string> algorithm_ids() const {
        std::vector<std::string> ids;
        ids.reserve(entries_.size());
        for (const auto& [algorithm_id, entry_value] : entries_) {
            (void)entry_value;
            ids.push_back(algorithm_id);
        }
        std::sort(ids.begin(), ids.end());
        return ids;
    }

    static std::string normalized_algorithm_id(std::string_view text) {
        std::string normalized;
        normalized.reserve(text.size());

        for (const char ch : text) {
            if (std::isspace(static_cast<unsigned char>(ch)) || ch == '-') {
                if (normalized.empty() || normalized.back() == '_') {
                    continue;
                }
                normalized.push_back('_');
                continue;
            }

            normalized.push_back(
                static_cast<char>(std::tolower(static_cast<unsigned char>(ch)))
            );
        }

        while (!normalized.empty() && normalized.back() == '_') {
            normalized.pop_back();
        }

        return normalized;
    }

private:
    std::unordered_map<std::string, entry> entries_;
};

} // namespace yodau::core

#endif // YODAU_CORE_ANALYSIS_PROCESSING_ALGORITHM_HPP
