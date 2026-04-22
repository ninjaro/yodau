#include "analysis/processing_algorithm_catalog.hpp"

#include "analysis/processing_algorithm_ids.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <system_error>
#include <utility>

namespace yodau::core {

namespace {

using namespace processing_algorithm_ids;

processing_parameter_descriptor parameter(
    std::string id, std::string display_name, std::string default_value,
    std::string unit = {},
    std::optional<double> min_value = std::optional<double> {},
    std::optional<double> max_value = std::optional<double> {},
    processing_parameter_visibility visibility
        = processing_parameter_visibility::advanced
) {
    return processing_parameter_descriptor {
        .id = std::move(id),
        .display_name = std::move(display_name),
        .default_value = std::move(default_value),
        .unit = std::move(unit),
        .min_value = min_value,
        .max_value = max_value,
        .visibility = visibility,
    };
}

bool token_matches(
    const std::string& normalized, const std::string& id,
    const std::vector<std::string>& aliases
) {
    if (normalized == id) {
        return true;
    }

    return std::ranges::any_of(
        aliases,
        [&normalized](const std::string& alias) {
            return normalized
                == processing_algorithm_registry::normalized_algorithm_id(alias);
        }
    );
}

const processing_algorithm_descriptor* descriptor_ptr_for(
    std::string_view algorithm_id
) {
    const std::string normalized
        = processing_algorithm_registry::normalized_algorithm_id(algorithm_id);

    for (const auto& descriptor : default_processing_algorithm_descriptors()) {
        if (token_matches(normalized, descriptor.id, descriptor.aliases)) {
            return &descriptor;
        }
    }

    return nullptr;
}

const processing_algorithm_descriptor& default_descriptor() {
    if (const auto* descriptor = descriptor_ptr_for(motion_baseline)) {
        return *descriptor;
    }

    return default_processing_algorithm_descriptors().front();
}

const processing_preset_descriptor& default_preset(
    const processing_algorithm_descriptor& descriptor
) {
    const auto it = std::ranges::find_if(
        descriptor.presets,
        [&descriptor](const processing_preset_descriptor& preset) {
            return preset.id == descriptor.default_preset_id;
        }
    );

    return it == descriptor.presets.end() ? descriptor.presets.front() : *it;
}

const processing_preset_descriptor* preset_ptr_for(
    const processing_algorithm_descriptor& descriptor, std::string_view preset_id
) {
    const std::string normalized
        = processing_algorithm_registry::normalized_algorithm_id(preset_id);

    for (const auto& preset : descriptor.presets) {
        if (token_matches(normalized, preset.id, preset.aliases)) {
            return &preset;
        }
    }

    return nullptr;
}

const processing_parameter_descriptor* parameter_ptr_for(
    const processing_algorithm_descriptor& descriptor,
    std::string_view parameter_id
) {
    const std::string normalized
        = processing_algorithm_registry::normalized_algorithm_id(parameter_id);

    const auto it = std::ranges::find_if(
        descriptor.parameters,
        [&normalized](const processing_parameter_descriptor& parameter_value) {
            return parameter_value.id == normalized;
        }
    );

    return it == descriptor.parameters.end() ? nullptr : &*it;
}

std::string parameter_value_to_string(
    const processing_algorithm_parameter_value& value
) {
    if (const auto* text = std::get_if<std::string>(&value)) {
        return *text;
    }
    if (const auto* flag = std::get_if<bool>(&value)) {
        return *flag ? "1" : "0";
    }
    if (const auto* integer = std::get_if<std::int64_t>(&value)) {
        return std::to_string(*integer);
    }
    if (const auto* number = std::get_if<double>(&value)) {
        if (std::isfinite(*number)) {
            std::string text(64, '\0');
            const auto [ptr, error] = std::to_chars(
                text.data(), text.data() + text.size(), *number
            );
            if (error == std::errc()) {
                text.resize(static_cast<size_t>(ptr - text.data()));
                return text;
            }
        }
        return std::to_string(*number);
    }

    return {};
}

} // namespace

const std::vector<processing_algorithm_descriptor>&
default_processing_algorithm_descriptors() {
    static const std::vector<processing_algorithm_descriptor> descriptors {
        processing_algorithm_descriptor {
            .id = std::string(motion_baseline),
            .display_name = "motion baseline",
            .aliases = { "default", "baseline", "motion" },
            .default_preset_id = "balanced",
            .presets = {
                processing_preset_descriptor {
                    .id = "simple",
                    .display_name = "simple",
                    .aliases = { "basic" },
                },
                processing_preset_descriptor {
                    .id = "balanced",
                    .display_name = "balanced",
                    .aliases = { "default" },
                },
                processing_preset_descriptor {
                    .id = "debug",
                    .display_name = "debug",
                    .aliases = { "debug_heavy" },
                },
            },
            .parameters = {
                parameter(
                    "pipeline_family", "pipeline family",
                    "motion_tripwire_baseline"
                ),
                parameter(
                    "overlay_contract", "overlay contract", "event_only"
                ),
            },
        },
        processing_algorithm_descriptor {
            .id = std::string(hybrid_auto),
            .display_name = "hybrid auto",
            .aliases = { "hybrid", "auto", "adaptive" },
            .default_preset_id = "adaptive",
            .presets = {
                processing_preset_descriptor {
                    .id = "load_guard",
                    .display_name = "load guard",
                    .aliases = { "simple", "basic" },
                    .configuration_values = {
                        { "probe_grid_cols", "10" },
                        { "probe_grid_rows", "10" },
                        { "diff_threshold", "30" },
                        { "calm_motion_permille", "40" },
                        { "busy_motion_permille", "180" },
                        { "overload_avg_ms", "6" },
                        { "recover_avg_ms", "4" },
                    },
                },
                processing_preset_descriptor {
                    .id = "adaptive",
                    .display_name = "adaptive",
                    .aliases = { "balanced", "default" },
                },
                processing_preset_descriptor {
                    .id = "tripwire_bias",
                    .display_name = "tripwire bias",
                    .aliases = { "debug", "debug_heavy" },
                    .configuration_values = {
                        { "diff_threshold", "20" },
                        { "calm_motion_permille", "12" },
                        { "busy_motion_permille", "140" },
                        { "overload_avg_ms", "18" },
                        { "recover_avg_ms", "10" },
                    },
                },
            },
            .parameters = {
                parameter(
                    "probe_grid_cols", "probe grid columns", "16", "cells",
                    2.0, 64.0, processing_parameter_visibility::basic
                ),
                parameter(
                    "probe_grid_rows", "probe grid rows", "16", "cells",
                    2.0, 64.0, processing_parameter_visibility::basic
                ),
                parameter(
                    "diff_threshold", "difference threshold", "24", "level",
                    1.0, 255.0, processing_parameter_visibility::basic
                ),
                parameter("blur_kernel", "blur kernel", "5", "px", 1.0, 31.0),
                parameter(
                    "calm_motion_permille", "calm motion threshold", "24",
                    "permille", 0.0, 1000.0,
                    processing_parameter_visibility::basic
                ),
                parameter(
                    "busy_motion_permille", "busy motion threshold", "110",
                    "permille", 0.0, 1000.0,
                    processing_parameter_visibility::basic
                ),
                parameter(
                    "overload_avg_ms", "overload average", "10", "ms", 0.0,
                    5000.0
                ),
                parameter(
                    "recover_avg_ms", "recover average", "6", "ms", 0.0,
                    5000.0
                ),
            },
        },
        processing_algorithm_descriptor {
            .id = std::string(spot_grid),
            .display_name = "spot grid",
            .aliases = { "spot", "spots" },
            .default_preset_id = "balanced",
            .presets = {
                processing_preset_descriptor {
                    .id = "coarse",
                    .display_name = "coarse",
                    .aliases = { "simple", "basic" },
                    .configuration_values = {
                        { "grid_cols", "8" },
                        { "grid_rows", "8" },
                        { "diff_threshold", "32" },
                        { "min_cell_energy", "40" },
                        { "blur_kernel", "7" },
                        { "emit_interval_ms", "200" },
                    },
                },
                processing_preset_descriptor {
                    .id = "balanced",
                    .display_name = "balanced",
                    .aliases = { "default" },
                },
                processing_preset_descriptor {
                    .id = "dense",
                    .display_name = "dense",
                    .aliases = { "debug", "debug_heavy" },
                    .configuration_values = {
                        { "grid_cols", "20" },
                        { "grid_rows", "20" },
                        { "diff_threshold", "20" },
                        { "min_cell_energy", "20" },
                        { "blur_kernel", "3" },
                        { "emit_interval_ms", "80" },
                    },
                },
            },
            .parameters = {
                parameter(
                    "grid_cols", "grid columns", "12", "cells", 2.0, 64.0,
                    processing_parameter_visibility::basic
                ),
                parameter(
                    "grid_rows", "grid rows", "12", "cells", 2.0, 64.0,
                    processing_parameter_visibility::basic
                ),
                parameter(
                    "diff_threshold", "difference threshold", "26", "level",
                    1.0, 255.0, processing_parameter_visibility::basic
                ),
                parameter(
                    "min_cell_energy", "minimum cell energy", "30", "level",
                    1.0, 255.0, processing_parameter_visibility::basic
                ),
                parameter("blur_kernel", "blur kernel", "5", "px", 1.0, 31.0),
                parameter(
                    "emit_interval_ms", "emit interval", "120", "ms", 0.0,
                    5000.0
                ),
            },
        },
        processing_algorithm_descriptor {
            .id = std::string(contour_mask),
            .display_name = "contour mask",
            .aliases = { "contour", "contours", "mask" },
            .default_preset_id = "outline",
            .presets = {
                processing_preset_descriptor {
                    .id = "outline",
                    .display_name = "outline",
                    .aliases = { "simple", "basic" },
                    .configuration_values = {
                        { "diff_threshold", "34" },
                        { "background_model", "frame_delta" },
                        { "morph_kernel", "3" },
                        { "min_contour_area", "220" },
                        { "emit_interval_ms", "220" },
                        { "max_overlays", "2" },
                        { "contour_points_limit", "16" },
                    },
                },
                processing_preset_descriptor {
                    .id = "balanced",
                    .display_name = "balanced",
                    .aliases = { "default" },
                },
                processing_preset_descriptor {
                    .id = "mask_heavy",
                    .display_name = "mask heavy",
                    .aliases = { "debug", "debug_heavy" },
                    .configuration_values = {
                        { "diff_threshold", "18" },
                        { "background_model", "mog2" },
                        { "background_history_frames", "240" },
                        { "background_threshold", "12" },
                        { "background_learning_permille", "3" },
                        { "morph_kernel", "7" },
                        { "min_contour_area", "72" },
                        { "emit_interval_ms", "80" },
                        { "max_overlays", "5" },
                        { "contour_points_limit", "48" },
                    },
                },
            },
            .parameters = {
                parameter(
                    "diff_threshold", "difference threshold", "28", "level",
                    1.0, 255.0, processing_parameter_visibility::basic
                ),
                parameter(
                    "background_model", "background model", "frame_delta",
                    "", std::optional<double> {}, std::optional<double> {},
                    processing_parameter_visibility::basic
                ),
                parameter(
                    "background_history_frames", "background history", "120",
                    "frames", 2.0, 10000.0
                ),
                parameter(
                    "background_threshold", "background threshold", "16",
                    "level", 1.0, 1000.0
                ),
                parameter(
                    "background_learning_permille", "background learning",
                    "5", "permille", 0.0, 1000.0
                ),
                parameter(
                    "background_detect_shadows", "background shadows", "0",
                    "bool", 0.0, 1.0
                ),
                parameter(
                    "motion_focus_mode", "motion focus", "auto", "",
                    std::optional<double> {}, std::optional<double> {},
                    processing_parameter_visibility::basic
                ),
                parameter(
                    "line_focus_width_pct", "line focus width", "8", "pct",
                    1.0, 100.0
                ),
                parameter("blur_kernel", "blur kernel", "5", "px", 1.0, 31.0),
                parameter(
                    "morph_kernel", "morphology kernel", "5", "px", 1.0,
                    31.0
                ),
                parameter(
                    "min_contour_area", "minimum contour area", "120",
                    "px2", 1.0, 1000000.0,
                    processing_parameter_visibility::basic
                ),
                parameter(
                    "emit_interval_ms", "emit interval", "150", "ms", 0.0,
                    5000.0
                ),
                parameter(
                    "max_overlays", "maximum overlays", "3", "overlays",
                    1.0, 8.0
                ),
                parameter(
                    "contour_points_limit", "contour point limit", "24",
                    "points", 3.0, 128.0
                ),
            },
        },
        processing_algorithm_descriptor {
            .id = std::string(centroid_track),
            .display_name = "centroid track",
            .aliases = {
                "track", "tracks", "tracker", "tracking", "centroid",
            },
            .default_preset_id = "balanced",
            .presets = {
                processing_preset_descriptor {
                    .id = "fast_match",
                    .display_name = "fast match",
                    .aliases = { "simple", "basic" },
                    .configuration_values = {
                        { "diff_threshold", "30" },
                        { "background_model", "frame_delta" },
                        { "sparse_flow_enabled", "1" },
                        { "sparse_flow_max_features", "48" },
                        { "track_velocity_prediction_pct", "45" },
                        { "track_gap_radius_growth_pct", "25" },
                        { "area_match_weight_pct", "10" },
                        { "match_radius_pct", "9" },
                        { "min_track_age_frames", "1" },
                        { "max_track_gap_frames", "1" },
                        { "track_history_limit", "5" },
                        { "max_overlays", "2" },
                        { "max_sparse_flow_overlays", "6" },
                        { "emit_interval_ms", "180" },
                    },
                },
                processing_preset_descriptor {
                    .id = "balanced",
                    .display_name = "balanced",
                    .aliases = { "default" },
                },
                processing_preset_descriptor {
                    .id = "persistent",
                    .display_name = "persistent",
                    .aliases = { "debug", "debug_heavy" },
                    .configuration_values = {
                        { "diff_threshold", "20" },
                        { "background_model", "mog2" },
                        { "background_history_frames", "240" },
                        { "background_threshold", "12" },
                        { "background_learning_permille", "3" },
                        { "sparse_flow_enabled", "1" },
                        { "sparse_flow_max_features", "140" },
                        { "sparse_flow_quality_permille", "8" },
                        { "sparse_flow_prediction_radius_pct", "18" },
                        { "track_velocity_prediction_pct", "70" },
                        { "track_gap_radius_growth_pct", "65" },
                        { "area_match_weight_pct", "24" },
                        { "match_radius_pct", "16" },
                        { "min_track_age_frames", "3" },
                        { "max_track_gap_frames", "8" },
                        { "track_history_limit", "24" },
                        { "max_overlays", "6" },
                        { "max_sparse_flow_overlays", "12" },
                        { "emit_interval_ms", "80" },
                    },
                },
            },
            .parameters = {
                parameter(
                    "diff_threshold", "difference threshold", "24", "level",
                    1.0, 255.0, processing_parameter_visibility::basic
                ),
                parameter(
                    "background_model", "background model", "frame_delta",
                    "", std::optional<double> {}, std::optional<double> {},
                    processing_parameter_visibility::basic
                ),
                parameter(
                    "background_history_frames", "background history", "120",
                    "frames", 2.0, 10000.0
                ),
                parameter(
                    "background_threshold", "background threshold", "16",
                    "level", 1.0, 1000.0
                ),
                parameter(
                    "background_learning_permille", "background learning",
                    "5", "permille", 0.0, 1000.0
                ),
                parameter(
                    "background_detect_shadows", "background shadows", "0",
                    "bool", 0.0, 1.0
                ),
                parameter(
                    "motion_focus_mode", "motion focus", "auto", "",
                    std::optional<double> {}, std::optional<double> {},
                    processing_parameter_visibility::basic
                ),
                parameter(
                    "line_focus_width_pct", "line focus width", "8", "pct",
                    1.0, 100.0
                ),
                parameter(
                    "sparse_flow_enabled", "sparse flow", "1", "bool", 0.0,
                    1.0, processing_parameter_visibility::basic
                ),
                parameter(
                    "sparse_flow_max_features", "sparse flow features", "80",
                    "features", 1.0, 1000.0
                ),
                parameter(
                    "sparse_flow_quality_permille", "sparse flow quality",
                    "12", "permille", 1.0, 1000.0
                ),
                parameter(
                    "sparse_flow_min_feature_distance_px",
                    "sparse flow feature spacing", "7", "px", 1.0, 512.0
                ),
                parameter(
                    "sparse_flow_window_px", "sparse flow window", "15",
                    "px", 3.0, 101.0
                ),
                parameter(
                    "sparse_flow_pyramid_levels", "sparse flow pyramid",
                    "3", "levels", 0.0, 8.0
                ),
                parameter(
                    "sparse_flow_max_error", "sparse flow max error", "24",
                    "error", 0.0, 1000.0
                ),
                parameter(
                    "sparse_flow_min_vector_px", "sparse flow min vector",
                    "2", "px", 0.0, 512.0
                ),
                parameter(
                    "sparse_flow_prediction_radius_pct",
                    "sparse flow prediction radius", "12", "pct", 1.0, 100.0
                ),
                parameter(
                    "track_velocity_prediction_pct",
                    "track velocity prediction weight", "55", "pct", 0.0,
                    100.0
                ),
                parameter(
                    "track_gap_radius_growth_pct",
                    "missed track gate growth", "40", "pct", 0.0, 200.0
                ),
                parameter(
                    "area_match_weight_pct", "area match weight", "18",
                    "pct", 0.0, 100.0
                ),
                parameter("blur_kernel", "blur kernel", "5", "px", 1.0, 31.0),
                parameter(
                    "morph_kernel", "morphology kernel", "5", "px", 1.0,
                    31.0
                ),
                parameter(
                    "min_contour_area", "minimum contour area", "96", "px2",
                    1.0, 1000000.0,
                    processing_parameter_visibility::basic
                ),
                parameter(
                    "match_radius_pct", "match radius", "12", "pct", 1.0,
                    50.0, processing_parameter_visibility::basic
                ),
                parameter(
                    "min_track_age_frames", "minimum track age", "2",
                    "frames", 1.0, 120.0
                ),
                parameter(
                    "max_track_gap_frames", "maximum track gap", "3",
                    "frames", 0.0, 120.0
                ),
                parameter(
                    "track_history_limit", "track history limit", "10",
                    "frames", 2.0, 64.0
                ),
                parameter(
                    "max_overlays", "maximum overlays", "4", "overlays",
                    1.0, 8.0
                ),
                parameter(
                    "max_sparse_flow_overlays",
                    "maximum sparse flow overlays", "8", "overlays", 0.0,
                    64.0
                ),
                parameter(
                    "emit_interval_ms", "emit interval", "120", "ms", 0.0,
                    5000.0
                ),
            },
        },
    };

    return descriptors;
}

std::vector<std::string> processing_algorithm_catalog_ids() {
    std::vector<std::string> ids;
    ids.reserve(default_processing_algorithm_descriptors().size());

    for (const auto& descriptor : default_processing_algorithm_descriptors()) {
        ids.push_back(descriptor.id);
    }

    return ids;
}

std::optional<processing_algorithm_descriptor> processing_algorithm_descriptor_for(
    std::string_view algorithm_id
) {
    const auto* descriptor = descriptor_ptr_for(algorithm_id);
    return descriptor == nullptr ? std::optional<processing_algorithm_descriptor> {}
                                 : *descriptor;
}

std::string normalized_processing_algorithm_id(std::string_view algorithm_id) {
    if (const auto* descriptor = descriptor_ptr_for(algorithm_id)) {
        return descriptor->id;
    }

    return default_descriptor().id;
}

std::string processing_algorithm_display_name(std::string_view algorithm_id) {
    if (const auto* descriptor = descriptor_ptr_for(algorithm_id)) {
        return descriptor->display_name;
    }

    return default_descriptor().display_name;
}

std::string processing_algorithm_default_preset_id(
    std::string_view algorithm_id
) {
    if (const auto* descriptor = descriptor_ptr_for(algorithm_id)) {
        return descriptor->default_preset_id;
    }

    return default_descriptor().default_preset_id;
}

processing_algorithm_configuration processing_algorithm_default_configuration(
    std::string_view algorithm_id
) {
    const auto* descriptor = descriptor_ptr_for(algorithm_id);
    if (descriptor == nullptr) {
        descriptor = &default_descriptor();
    }

    processing_algorithm_configuration configuration;
    for (const auto& parameter_value : descriptor->parameters) {
        configuration.values.emplace(
            parameter_value.id, parameter_value.default_value
        );
    }

    return configuration;
}

processing_algorithm_configuration completed_processing_configuration(
    std::string_view algorithm_id,
    processing_algorithm_configuration configuration
) {
    const processing_algorithm_configuration defaults
        = processing_algorithm_default_configuration(algorithm_id);
    for (const auto& [key, value] : defaults.values) {
        if (!configuration.values.contains(key)) {
            configuration.values.emplace(key, value);
        }
    }

    return configuration;
}

processing_algorithm_configuration processing_algorithm_preset_configuration(
    std::string_view algorithm_id, std::string_view preset_id
) {
    const auto* descriptor = descriptor_ptr_for(algorithm_id);
    if (descriptor == nullptr) {
        descriptor = &default_descriptor();
    }

    processing_algorithm_configuration configuration
        = processing_algorithm_default_configuration(descriptor->id);
    const auto* preset = preset_ptr_for(*descriptor, preset_id);
    if (preset == nullptr) {
        preset = &default_preset(*descriptor);
    }

    for (const auto& [key, value] : preset->configuration_values) {
        if (parameter_ptr_for(*descriptor, key) != nullptr) {
            configuration.values.insert_or_assign(key, value);
        }
    }

    return configuration;
}

processing_algorithm_settings default_processing_algorithm_settings(
    std::string_view algorithm_id
) {
    processing_algorithm_settings settings;
    settings.algorithm_id = normalized_processing_algorithm_id(algorithm_id);
    if (!settings.algorithm_id.empty()) {
        settings.preset_id
            = processing_algorithm_default_preset_id(settings.algorithm_id);
    }
    return settings;
}

processing_algorithm_settings normalized_processing_algorithm_settings(
    processing_algorithm_settings settings
) {
    if (settings.algorithm_id.empty()) {
        settings.preset_id.clear();
        settings.parameter_overrides.clear();
        return settings;
    }

    settings.algorithm_id = normalized_processing_algorithm_id(
        settings.algorithm_id
    );
    settings.preset_id = settings.preset_id.empty()
        ? processing_algorithm_default_preset_id(settings.algorithm_id)
        : normalized_processing_algorithm_preset_id(
              settings.algorithm_id, settings.preset_id
          );

    const auto* descriptor = descriptor_ptr_for(settings.algorithm_id);
    if (descriptor == nullptr) {
        descriptor = &default_descriptor();
    }

    std::unordered_map<std::string, processing_algorithm_parameter_value>
        normalized_overrides;
    for (auto& [key, value] : settings.parameter_overrides) {
        const std::string normalized_key
            = processing_algorithm_registry::normalized_algorithm_id(key);
        if (const auto* parameter_value
            = parameter_ptr_for(*descriptor, normalized_key)) {
            normalized_overrides.insert_or_assign(
                parameter_value->id, std::move(value)
            );
        }
    }
    settings.parameter_overrides = std::move(normalized_overrides);
    return settings;
}

processing_algorithm_configuration processing_algorithm_settings_configuration(
    const processing_algorithm_settings& settings
) {
    const processing_algorithm_settings normalized_settings
        = normalized_processing_algorithm_settings(settings);
    if (normalized_settings.algorithm_id.empty()) {
        return {};
    }

    processing_algorithm_configuration configuration
        = processing_algorithm_preset_configuration(
            normalized_settings.algorithm_id, normalized_settings.preset_id
        );

    for (const auto& [key, value] : normalized_settings.parameter_overrides) {
        configuration.values.insert_or_assign(
            key, parameter_value_to_string(value)
        );
    }

    return configuration;
}

std::vector<std::string> processing_algorithm_preset_ids(
    std::string_view algorithm_id
) {
    const auto* descriptor = descriptor_ptr_for(algorithm_id);
    if (descriptor == nullptr) {
        descriptor = &default_descriptor();
    }

    std::vector<std::string> ids;
    ids.reserve(descriptor->presets.size());

    for (const auto& preset : descriptor->presets) {
        ids.push_back(preset.id);
    }

    return ids;
}

std::optional<processing_preset_descriptor> processing_preset_descriptor_for(
    std::string_view algorithm_id, std::string_view preset_id
) {
    const auto* descriptor = descriptor_ptr_for(algorithm_id);
    if (descriptor == nullptr) {
        descriptor = &default_descriptor();
    }

    const auto* preset = preset_ptr_for(*descriptor, preset_id);
    return preset == nullptr ? std::optional<processing_preset_descriptor> {}
                             : *preset;
}

std::string normalized_processing_algorithm_preset_id(
    std::string_view algorithm_id, std::string_view preset_id
) {
    const auto* descriptor = descriptor_ptr_for(algorithm_id);
    if (descriptor == nullptr) {
        descriptor = &default_descriptor();
    }

    if (const auto* preset = preset_ptr_for(*descriptor, preset_id)) {
        return preset->id;
    }

    return default_preset(*descriptor).id;
}

std::string processing_algorithm_preset_display_name(
    std::string_view algorithm_id, std::string_view preset_id
) {
    const auto* descriptor = descriptor_ptr_for(algorithm_id);
    if (descriptor == nullptr) {
        descriptor = &default_descriptor();
    }

    const std::string normalized_preset_id
        = normalized_processing_algorithm_preset_id(descriptor->id, preset_id);
    if (const auto* preset = preset_ptr_for(*descriptor, normalized_preset_id)) {
        return preset->display_name;
    }

    return default_preset(*descriptor).display_name;
}

std::optional<processing_parameter_descriptor> processing_parameter_descriptor_for(
    std::string_view algorithm_id, std::string_view parameter_id
) {
    const auto* descriptor = descriptor_ptr_for(algorithm_id);
    if (descriptor == nullptr) {
        descriptor = &default_descriptor();
    }

    const auto* parameter_value = parameter_ptr_for(*descriptor, parameter_id);
    return parameter_value == nullptr
        ? std::optional<processing_parameter_descriptor> {}
        : *parameter_value;
}

} // namespace yodau::core
