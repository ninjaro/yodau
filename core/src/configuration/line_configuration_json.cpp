#include "configuration/line_configuration_json.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <ranges>
#include <utility>

namespace yodau::core {
namespace {

    using json = nlohmann::json;

    constexpr size_t maximum_configuration_bytes { 4U * 1024U * 1024U };
    constexpr size_t maximum_lines { 4096U };

    [[noreturn]] void fail(const std::string& message) {
        throw line_configuration_error(message);
    }

    void require_object(const json& value, const std::string& location) {
        if (!value.is_object()) {
            fail(location + " must be an object");
        }
    }

    void require_exact_keys(
        const json& value, std::initializer_list<std::string_view> allowed,
        const std::string& location
    ) {
        require_object(value, location);
        for (const auto& [key, ignored] : value.items()) {
            static_cast<void>(ignored);
            if (std::ranges::none_of(
                    allowed, [&key](const std::string_view candidate) {
                        return candidate == key;
                    }
                )) {
                std::string message = location;
                message.append(" contains unknown field '");
                message.append(key);
                message.push_back('\'');
                fail(message);
            }
        }
        for (const std::string_view required : allowed) {
            if (!value.contains(required)) {
                fail(
                    location + " is missing required field '"
                    + std::string(required) + "'"
                );
            }
        }
    }

    template <typename T>
    T required_value(
        const json& object, const std::string_view key,
        const std::string& location
    ) {
        try {
            return object.at(key).get<T>();
        } catch (const json::exception& error) {
            fail(
                location + "." + std::string(key)
                + " has the wrong type: " + error.what()
            );
        }
    }

    json
    parameter_value_json(const processing_algorithm_parameter_value& value) {
        return std::visit([](const auto& item) -> json { return item; }, value);
    }

    processing_algorithm_parameter_value
    parse_parameter_value(const json& value, const std::string& location) {
        if (value.is_boolean()) {
            return value.get<bool>();
        }
        if (value.is_number_integer()) {
            return value.get<std::int64_t>();
        }
        if (value.is_number_float()) {
            const double number = value.get<double>();
            if (!std::isfinite(number)) {
                fail(location + " must be finite");
            }
            processing_algorithm_parameter_value parsed {
                std::in_place_type<double>, number
            };
            return parsed;
        }
        if (value.is_string()) {
            return value.get<std::string>();
        }
        fail(location + " must be a string, boolean, integer, or number");
    }

    json point_json(const point& value) {
        return json { { "x", value.x }, { "y", value.y } };
    }

    point parse_point(const json& value, const std::string& location) {
        require_exact_keys(value, { "x", "y" }, location);
        return point {
            .x = required_value<float>(value, "x", location),
            .y = required_value<float>(value, "y", location),
        };
    }

    json profile_json(const line_profile& value) {
        return json {
            { "visual_width", value.visual_width },
            { "interaction_width", value.interaction_width },
            { "effective_length", value.effective_length },
            { "damping", value.damping },
        };
    }

    line_profile parse_profile(
        const json& value, const std::string& line_name,
        const std::string& location
    ) {
        require_exact_keys(
            value,
            { "visual_width", "interaction_width", "effective_length",
              "damping" },
            location
        );
        return line_profile {
            .line_name = line_name,
            .visual_width
            = required_value<float>(value, "visual_width", location),
            .interaction_width
            = required_value<float>(value, "interaction_width", location),
            .effective_length
            = required_value<float>(value, "effective_length", location),
            .damping = required_value<float>(value, "damping", location),
        };
    }

    json appearance_json(const line_configuration_appearance& value) {
        return json {
            { "color", value.color },
            { "color_mode", value.color_mode },
            { "width_text", value.width_text },
            { "length_text", value.length_text },
            { "response_text", value.response_text },
        };
    }

    line_configuration_appearance
    parse_appearance(const json& value, const std::string& location) {
        require_exact_keys(
            value,
            { "color", "color_mode", "width_text", "length_text",
              "response_text" },
            location
        );
        return line_configuration_appearance {
            .color = required_value<std::string>(value, "color", location),
            .color_mode
            = required_value<std::string>(value, "color_mode", location),
            .width_text
            = required_value<std::string>(value, "width_text", location),
            .length_text
            = required_value<std::string>(value, "length_text", location),
            .response_text
            = required_value<std::string>(value, "response_text", location),
        };
    }

    json algorithm_json(const processing_algorithm_settings& value) {
        json parameters = json::object();
        for (const auto& [name, parameter_value] : value.parameter_overrides) {
            parameters[name] = parameter_value_json(parameter_value);
        }
        return json {
            { "id", value.algorithm_id },
            { "preset", value.preset_id },
            { "parameters", std::move(parameters) },
        };
    }

    processing_algorithm_settings
    parse_algorithm(const json& value, const std::string& location) {
        require_exact_keys(value, { "id", "preset", "parameters" }, location);
        processing_algorithm_settings settings {
            .algorithm_id = required_value<std::string>(value, "id", location),
            .preset_id = required_value<std::string>(value, "preset", location),
            .parameter_overrides = {},
        };
        const json& parameters = value.at("parameters");
        require_object(parameters, location + ".parameters");
        for (const auto& [name, parameter_value] : parameters.items()) {
            std::string parameter_location = location;
            parameter_location.append(".parameters.");
            parameter_location.append(name);
            settings.parameter_overrides.emplace(
                name, parse_parameter_value(parameter_value, parameter_location)
            );
        }
        return settings;
    }

    json configured_line_json(const configured_line& value) {
        json points = json::array();
        for (const point& point_value : value.points) {
            points.push_back(point_json(point_value));
        }
        return json {
            { "name", value.name },
            { "enabled", value.enabled },
            { "closed", value.closed },
            { "direction", tripwire_direction_name(value.direction) },
            { "points", std::move(points) },
            { "profile", profile_json(value.profile) },
            { "appearance", appearance_json(value.appearance) },
        };
    }

    configured_line
    parse_configured_line(const json& value, const size_t index) {
        const std::string location = "lines[" + std::to_string(index) + "]";
        require_exact_keys(
            value,
            { "name", "enabled", "closed", "direction", "points", "profile",
              "appearance" },
            location
        );
        configured_line line_value;
        line_value.name = required_value<std::string>(value, "name", location);
        line_value.enabled = required_value<bool>(value, "enabled", location);
        line_value.closed = required_value<bool>(value, "closed", location);
        line_value.direction = parse_tripwire_direction(
            required_value<std::string>(value, "direction", location)
        );
        const json& points = value.at("points");
        if (!points.is_array()) {
            fail(location + ".points must be an array");
        }
        line_value.points.reserve(points.size());
        for (size_t point_index = 0; point_index < points.size();
             ++point_index) {
            line_value.points.push_back(parse_point(
                points.at(point_index),
                location + ".points[" + std::to_string(point_index) + "]"
            ));
        }
        line_value.profile = parse_profile(
            value.at("profile"), line_value.name, location + ".profile"
        );
        line_value.appearance = parse_appearance(
            value.at("appearance"), location + ".appearance"
        );
        return line_value;
    }

    json stream_json(const line_configuration_stream& value) {
        return json {
            { "name", value.name },
            { "source", value.source },
            { "type", value.type },
            { "loop", value.loop },
            { "virtual_camera", value.virtual_camera_path },
            { "analysis_interval_ms", value.analysis_interval_ms },
            { "algorithm", algorithm_json(value.algorithm) },
            { "display",
              {
                  { "labels_enabled", value.labels_enabled },
                  { "standard_labels_enabled", value.standard_labels_enabled },
                  { "movement_display_mode", value.movement_display_mode },
                  { "manual_processing_policy_enabled",
                    value.manual_processing_policy_enabled },
                  { "manual_display_fps", value.manual_display_fps },
                  { "manual_core_fps", value.manual_core_fps },
                  { "manual_processing_pixels",
                    value.manual_processing_pixels },
              } },
        };
    }

    line_configuration_stream
    parse_stream(const json& value, const std::string& location) {
        require_exact_keys(
            value,
            { "name", "source", "type", "loop", "virtual_camera",
              "analysis_interval_ms", "algorithm", "display" },
            location
        );
        const json& display = value.at("display");
        require_exact_keys(
            display,
            { "labels_enabled", "standard_labels_enabled",
              "movement_display_mode", "manual_processing_policy_enabled",
              "manual_display_fps", "manual_core_fps",
              "manual_processing_pixels" },
            location + ".display"
        );
        return line_configuration_stream {
            .name = required_value<std::string>(value, "name", location),
            .source = required_value<std::string>(value, "source", location),
            .type = required_value<std::string>(value, "type", location),
            .loop = required_value<bool>(value, "loop", location),
            .virtual_camera_path
            = required_value<std::string>(value, "virtual_camera", location),
            .analysis_interval_ms
            = required_value<int>(value, "analysis_interval_ms", location),
            .algorithm
            = parse_algorithm(value.at("algorithm"), location + ".algorithm"),
            .labels_enabled = required_value<bool>(
                display, "labels_enabled", location + ".display"
            ),
            .standard_labels_enabled = required_value<bool>(
                display, "standard_labels_enabled", location + ".display"
            ),
            .movement_display_mode = required_value<std::string>(
                display, "movement_display_mode", location + ".display"
            ),
            .manual_processing_policy_enabled = required_value<bool>(
                display, "manual_processing_policy_enabled",
                location + ".display"
            ),
            .manual_display_fps = required_value<int>(
                display, "manual_display_fps", location + ".display"
            ),
            .manual_core_fps = required_value<int>(
                display, "manual_core_fps", location + ".display"
            ),
            .manual_processing_pixels = required_value<int>(
                display, "manual_processing_pixels", location + ".display"
            ),
        };
    }

} // namespace

std::string encode_line_configuration_json(
    const line_configuration_document& document
) {
    validate_line_configuration(document);
    json lines = json::array();
    for (const configured_line& line_value : document.lines) {
        lines.push_back(configured_line_json(line_value));
    }
    const json root {
        { "format", line_configuration_format },
        { "stream", stream_json(document.stream) },
        { "lines", std::move(lines) },
    };
    std::string encoded = root.dump(2) + '\n';
    if (encoded.size() > maximum_configuration_bytes) {
        fail("serialized line configuration exceeds the 4 MiB size limit");
    }
    return encoded;
}

line_configuration_document
decode_line_configuration_json(const std::string_view contents) {
    if (contents.empty()) {
        fail("line configuration is empty");
    }
    if (contents.size() > maximum_configuration_bytes) {
        fail("line configuration exceeds the 4 MiB size limit");
    }

    try {
        const json root = json::parse(contents.begin(), contents.end());
        require_exact_keys(
            root, { "format", "stream", "lines" }, "root"
        );
        if (required_value<std::string>(root, "format", "root")
            != line_configuration_format) {
            fail("root.format is not a yodau line configuration");
        }

        line_configuration_document document;
        document.stream = parse_stream(root.at("stream"), "stream");
        const json& lines = root.at("lines");
        if (!lines.is_array()) {
            fail("lines must be an array");
        }
        if (lines.size() > maximum_lines) {
            fail("lines exceeds the supported limit");
        }
        document.lines.reserve(lines.size());
        for (size_t index = 0; index < lines.size(); ++index) {
            document.lines.push_back(
                parse_configured_line(lines.at(index), index)
            );
        }
        validate_line_configuration(document);
        return document;
    } catch (const line_configuration_error&) {
        throw;
    } catch (const json::exception& error) {
        fail(std::string("invalid line configuration JSON: ") + error.what());
    }
}

} // namespace yodau::core
