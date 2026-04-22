#ifndef YODAU_CORE_ANALYSIS_PROCESSING_ALGORITHM_CATALOG_HPP
#define YODAU_CORE_ANALYSIS_PROCESSING_ALGORITHM_CATALOG_HPP

#include "analysis/processing_algorithm.hpp"
#include "core/namespace_alias.hpp"

#include <optional>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace yodau::core {

struct processing_preset_descriptor {
    std::string id;
    std::string display_name;
    std::vector<std::string> aliases;
    std::unordered_map<std::string, std::string> configuration_values;
};

enum class processing_parameter_visibility { basic, advanced };

struct processing_parameter_descriptor {
    std::string id;
    std::string display_name;
    std::string default_value;
    std::string unit;
    std::optional<double> min_value;
    std::optional<double> max_value;
    processing_parameter_visibility visibility {
        processing_parameter_visibility::advanced
    };
};

struct processing_algorithm_descriptor {
    std::string id;
    std::string display_name;
    std::vector<std::string> aliases;
    std::string default_preset_id;
    std::vector<processing_preset_descriptor> presets;
    std::vector<processing_parameter_descriptor> parameters;
};

using processing_algorithm_parameter_value = std::variant<
    std::string,
    bool,
    std::int64_t,
    double
>;

struct processing_algorithm_settings {
    std::string algorithm_id;
    std::string preset_id;
    std::unordered_map<std::string, processing_algorithm_parameter_value>
        parameter_overrides;

    bool operator==(const processing_algorithm_settings& other) const = default;
};

const std::vector<processing_algorithm_descriptor>&
default_processing_algorithm_descriptors();

std::vector<std::string> processing_algorithm_catalog_ids();

std::optional<processing_algorithm_descriptor> processing_algorithm_descriptor_for(
    std::string_view algorithm_id
);

std::string normalized_processing_algorithm_id(std::string_view algorithm_id);

std::string processing_algorithm_display_name(std::string_view algorithm_id);

std::string processing_algorithm_default_preset_id(std::string_view algorithm_id);

processing_algorithm_configuration processing_algorithm_default_configuration(
    std::string_view algorithm_id
);

processing_algorithm_configuration completed_processing_configuration(
    std::string_view algorithm_id,
    processing_algorithm_configuration configuration
);

processing_algorithm_configuration processing_algorithm_preset_configuration(
    std::string_view algorithm_id, std::string_view preset_id
);

processing_algorithm_settings default_processing_algorithm_settings(
    std::string_view algorithm_id
);

processing_algorithm_settings normalized_processing_algorithm_settings(
    processing_algorithm_settings settings
);

processing_algorithm_configuration processing_algorithm_settings_configuration(
    const processing_algorithm_settings& settings
);

std::vector<std::string> processing_algorithm_preset_ids(
    std::string_view algorithm_id
);

std::optional<processing_preset_descriptor> processing_preset_descriptor_for(
    std::string_view algorithm_id, std::string_view preset_id
);

std::string normalized_processing_algorithm_preset_id(
    std::string_view algorithm_id, std::string_view preset_id
);

std::string processing_algorithm_preset_display_name(
    std::string_view algorithm_id, std::string_view preset_id
);

std::optional<processing_parameter_descriptor> processing_parameter_descriptor_for(
    std::string_view algorithm_id, std::string_view parameter_id
);

} // namespace yodau::core

#endif // YODAU_CORE_ANALYSIS_PROCESSING_ALGORITHM_CATALOG_HPP
