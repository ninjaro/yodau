#ifndef YODAU_CORE_CONFIGURATION_LINE_CONFIGURATION_MODEL_HPP
#define YODAU_CORE_CONFIGURATION_LINE_CONFIGURATION_MODEL_HPP

#include "analysis/processing_algorithm_catalog.hpp"
#include "geometry/geometry.hpp"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace yodau::core {

class processing_runtime;
class stream_manager;

class line_configuration_error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct line_configuration_appearance {
    std::string color { "#ffff0000" };
    std::string color_mode { "auto_palette" };
    std::string width_text { "1.0" };
    std::string length_text { "1.0" };
    std::string response_text { "0.5" };

    bool operator==(const line_configuration_appearance&) const = default;
};

struct configured_line {
    std::string name;
    std::vector<point> points;
    bool closed { false };
    tripwire_dir direction { tripwire_dir::any };
    bool enabled { true };
    line_profile profile;
    line_configuration_appearance appearance;

    bool operator==(const configured_line& other) const;
};

struct line_configuration_stream {
    std::string name;
    std::string source;
    std::string type;
    bool loop { true };
    std::string virtual_camera_path { "/dev/yodau0" };
    int analysis_interval_ms { 0 };
    processing_algorithm_settings algorithm;

    // These values are part of the document so the desktop app can round-trip
    // operator choices without making the headless runtime depend on Qt.
    bool labels_enabled { true };
    bool standard_labels_enabled { true };
    std::string movement_display_mode { "auto" };
    bool manual_processing_policy_enabled { false };
    int manual_display_fps { 24 };
    int manual_core_fps { 12 };
    int manual_processing_pixels { 1280 * 720 };

    bool operator==(const line_configuration_stream&) const = default;
};

struct line_configuration_document {
    line_configuration_stream stream;
    std::vector<configured_line> lines;

    bool operator==(const line_configuration_document&) const = default;
};

struct line_configuration_apply_options {
    std::optional<std::string> stream_name_override;
    std::optional<std::string> source_override;
    std::optional<std::string> source_type_override;
    std::optional<bool> loop_override;
    std::optional<std::string> virtual_camera_path_override;
};

struct line_configuration_apply_result {
    std::string stream_name;
    std::string source;
    std::string virtual_camera_path;
    size_t connected_line_count { 0 };
};

void validate_line_configuration(const line_configuration_document& document);

[[nodiscard]] line_configuration_apply_result apply_line_configuration(
    const line_configuration_document& document, stream_manager& manager,
    processing_runtime& runtime,
    const line_configuration_apply_options& options = {}
);

[[nodiscard]] std::string tripwire_direction_name(tripwire_dir direction);
[[nodiscard]] tripwire_dir parse_tripwire_direction(std::string_view value);

} // namespace yodau::core

#endif // YODAU_CORE_CONFIGURATION_LINE_CONFIGURATION_MODEL_HPP
