#ifndef YODAU_CORE_ANALYSIS_PROCESSING_ALGORITHM_CATALOG_HPP
#define YODAU_CORE_ANALYSIS_PROCESSING_ALGORITHM_CATALOG_HPP

#include "analysis/processing_algorithm.hpp"
#include "core/namespace_alias.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace yodau::core {

struct processing_preset_descriptor {
    std::string id;
    std::string display_name;
    std::vector<std::string> aliases;
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
