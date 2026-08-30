#include "configuration/line_configuration_model.hpp"

#include "analysis/processing_runtime.hpp"
#include "streams/stream_manager.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <ranges>
#include <unordered_set>

namespace yodau::core {
namespace {

constexpr std::size_t maximum_lines { 4096U };
constexpr std::size_t maximum_points_per_line { 16384U };
constexpr std::size_t maximum_total_points { 100000U };

[[noreturn]] void fail(const std::string& message) {
    throw line_configuration_error(message);
}

bool has_control_character(const std::string_view text) {
    return std::ranges::any_of(text, [](const unsigned char value) {
        return value < 0x20U || value == 0x7fU;
    });
}

void validate_text(
    const std::string& value, const std::string& location,
    const std::size_t maximum_size, const bool allow_empty = false
) {
    if ((!allow_empty && value.empty()) || value.size() > maximum_size
        || has_control_character(value)) {
        fail(
            location + " must be " + (allow_empty ? "a" : "a non-empty")
            + " printable string of at most " + std::to_string(maximum_size)
            + " bytes"
        );
    }
}

void validate_canonical_name(
    const std::string& value, const std::string& location
) {
    if (!value.empty()
        && (std::isspace(static_cast<unsigned char>(value.front())) != 0
            || std::isspace(static_cast<unsigned char>(value.back())) != 0)) {
        fail(location + " must not have leading or trailing whitespace");
    }
}

bool valid_color(const std::string& value) {
    if ((value.size() != 7U && value.size() != 9U) || value.front() != '#') {
        return false;
    }
    return std::ranges::all_of(value.substr(1), [](const unsigned char ch) {
        return std::isxdigit(ch) != 0;
    });
}

void validate_parameter_value(
    const processing_parameter_descriptor& descriptor,
    const processing_algorithm_parameter_value& value,
    const std::string& location
) {
    switch (descriptor.kind) {
    case processing_parameter_kind::boolean:
        if (!std::holds_alternative<bool>(value)) {
            fail(location + " must be a boolean");
        }
        break;
    case processing_parameter_kind::integer:
        if (!std::holds_alternative<std::int64_t>(value)) {
            fail(location + " must be an integer");
        }
        break;
    case processing_parameter_kind::real:
        if (!std::holds_alternative<double>(value)
            && !std::holds_alternative<std::int64_t>(value)) {
            fail(location + " must be a number");
        }
        break;
    case processing_parameter_kind::text:
        if (!std::holds_alternative<std::string>(value)) {
            fail(location + " must be a string");
        }
        break;
    }

    std::optional<double> numeric;
    if (const auto* integer = std::get_if<std::int64_t>(&value)) {
        numeric = static_cast<double>(*integer);
    } else if (const auto* real = std::get_if<double>(&value)) {
        if (!std::isfinite(*real)) {
            fail(location + " must be finite");
        }
        numeric = *real;
    } else if (const auto* text = std::get_if<std::string>(&value)) {
        validate_text(*text, location, 1024U, true);
        if (!descriptor.allowed_values.empty()
            && std::ranges::find(descriptor.allowed_values, *text)
                == descriptor.allowed_values.end()) {
            fail(location + " contains an unsupported value '" + *text + "'");
        }
    }

    if (numeric.has_value() && descriptor.min_value.has_value()
        && *numeric < *descriptor.min_value) {
        fail(location + " is below the supported minimum");
    }
    if (numeric.has_value() && descriptor.max_value.has_value()
        && *numeric > *descriptor.max_value) {
        fail(location + " is above the supported maximum");
    }
}

bool same_geometry(const line& existing, const configured_line& configured) {
    if (existing.closed != configured.closed
        || existing.points.size() != configured.points.size()) {
        return false;
    }
    for (std::size_t index = 0; index < existing.points.size(); ++index) {
        if (!existing.points[index].compare(configured.points[index])) {
            return false;
        }
    }
    return true;
}

} // namespace

std::string tripwire_direction_name(const tripwire_dir direction) {
    switch (direction) {
    case tripwire_dir::any:
        return "any";
    case tripwire_dir::neg_to_pos:
        return "negative_to_positive";
    case tripwire_dir::pos_to_neg:
        return "positive_to_negative";
    }
    fail("invalid tripwire direction");
}

tripwire_dir parse_tripwire_direction(const std::string_view value) {
    if (value == "any") {
        return tripwire_dir::any;
    }
    if (value == "negative_to_positive") {
        return tripwire_dir::neg_to_pos;
    }
    if (value == "positive_to_negative") {
        return tripwire_dir::pos_to_neg;
    }
    fail("unsupported tripwire direction '" + std::string(value) + "'");
}

bool configured_line::operator==(const configured_line& other) const {
    if (name != other.name || closed != other.closed
        || direction != other.direction || enabled != other.enabled
        || !(profile == other.profile) || appearance != other.appearance
        || points.size() != other.points.size()) {
        return false;
    }
    for (std::size_t index = 0; index < points.size(); ++index) {
        if (!points[index].compare(other.points[index])) {
            return false;
        }
    }
    return true;
}

void validate_line_configuration(const line_configuration_document& document) {
    const auto& stream_value = document.stream;
    validate_text(stream_value.name, "stream.name", 256U);
    validate_canonical_name(stream_value.name, "stream.name");
    validate_text(stream_value.source, "stream.source", 4096U);
    validate_text(stream_value.type, "stream.type", 16U);
    if (stream_value.type != "local" && stream_value.type != "file"
        && stream_value.type != "rtsp" && stream_value.type != "http") {
        fail("stream.type must be local, file, rtsp, or http");
    }
    validate_text(
        stream_value.virtual_camera_path, "stream.virtual_camera", 4096U, true
    );
    if (stream_value.analysis_interval_ms < 0
        || stream_value.analysis_interval_ms > 60000) {
        fail("stream.analysis_interval_ms must be between 0 and 60000");
    }
    validate_text(
        stream_value.movement_display_mode,
        "stream.display.movement_display_mode", 64U
    );
    if (stream_value.manual_display_fps < 1
        || stream_value.manual_display_fps > 120
        || stream_value.manual_core_fps < 1
        || stream_value.manual_core_fps > 120) {
        fail("stream display frame rates must be between 1 and 120");
    }
    if (stream_value.manual_processing_pixels < 16 * 16
        || stream_value.manual_processing_pixels > 7680 * 4320) {
        fail(
            "stream.display.manual_processing_pixels is outside supported "
            "bounds"
        );
    }

    const auto descriptor = processing_algorithm_descriptor_for(
        stream_value.algorithm.algorithm_id
    );
    if (!descriptor.has_value()
        || descriptor->id != stream_value.algorithm.algorithm_id) {
        fail("stream.algorithm.id must be a canonical supported algorithm id");
    }
    const auto preset = processing_preset_descriptor_for(
        descriptor->id, stream_value.algorithm.preset_id
    );
    if (!preset.has_value() || preset->id != stream_value.algorithm.preset_id) {
        fail("stream.algorithm.preset must be a canonical supported preset id");
    }
    for (const auto& [name, value] :
         stream_value.algorithm.parameter_overrides) {
        const auto parameter
            = processing_parameter_descriptor_for(descriptor->id, name);
        if (!parameter.has_value() || parameter->id != name) {
            fail(
                "stream.algorithm.parameters contains unsupported key '" + name
                + "'"
            );
        }
        validate_parameter_value(
            *parameter, value, "stream.algorithm.parameters." + name
        );
    }

    if (document.lines.size() > maximum_lines) {
        fail("lines exceeds the supported limit");
    }
    std::unordered_set<std::string> names;
    std::size_t total_points = 0U;
    for (std::size_t index = 0; index < document.lines.size(); ++index) {
        const auto& line_value = document.lines[index];
        const std::string location = "lines[" + std::to_string(index) + "]";
        validate_text(line_value.name, location + ".name", 256U);
        validate_canonical_name(line_value.name, location + ".name");
        if (!names.insert(line_value.name).second) {
            fail(location + ".name duplicates another line");
        }
        if (line_value.points.size() > maximum_points_per_line) {
            fail(location + ".points has an invalid point count");
        }
        total_points += line_value.points.size();
        if (total_points > maximum_total_points) {
            fail("lines contain too many points");
        }
        try {
            validate_line_geometry(
                line_value.points, line_value.name, line_value.closed
            );
        } catch (const std::invalid_argument& error) {
            fail(location + ": " + error.what());
        }

        const line_profile& profile = line_value.profile;
        if (profile.line_name != line_value.name) {
            fail(location + ".profile line identity does not match line name");
        }
        if (!std::isfinite(profile.visual_width)
            || !std::isfinite(profile.interaction_width)
            || !std::isfinite(profile.effective_length)
            || !std::isfinite(profile.damping) || profile.visual_width <= 0.0f
            || profile.interaction_width <= 0.0f
            || profile.effective_length <= 0.0f || profile.damping < 0.0f
            || profile.damping > 1.0f || profile.visual_width > 100.0f
            || profile.interaction_width > 100.0f
            || profile.effective_length > 100.0f) {
            fail(location + ".profile contains invalid response values");
        }
        if (!valid_color(line_value.appearance.color)) {
            fail(location + ".appearance.color must be #RRGGBB or #AARRGGBB");
        }
        validate_text(
            line_value.appearance.color_mode,
            location + ".appearance.color_mode", 64U
        );
        validate_text(
            line_value.appearance.width_text,
            location + ".appearance.width_text", 64U
        );
        validate_text(
            line_value.appearance.length_text,
            location + ".appearance.length_text", 64U
        );
        validate_text(
            line_value.appearance.response_text,
            location + ".appearance.response_text", 64U
        );
    }
}

line_configuration_apply_result apply_line_configuration(
    const line_configuration_document& document, stream_manager& manager,
    processing_runtime& runtime, const line_configuration_apply_options& options
) {
    validate_line_configuration(document);

    const std::string stream_name
        = options.stream_name_override.value_or(document.stream.name);
    const std::string source
        = options.source_override.value_or(document.stream.source);
    const std::string source_type
        = options.source_type_override.value_or(document.stream.type);
    const bool loop = options.loop_override.value_or(document.stream.loop);
    const std::string output_path
        = options.virtual_camera_path_override.value_or(
            document.stream.virtual_camera_path
        );
    validate_text(stream_name, "resolved stream name", 256U);
    validate_canonical_name(stream_name, "resolved stream name");
    validate_text(source, "resolved source", 4096U);
    validate_text(source_type, "resolved stream type", 16U);
    if (source_type != "local" && source_type != "file"
        && source_type != "rtsp" && source_type != "http") {
        fail("resolved stream type must be local, file, rtsp, or http");
    }
    validate_text(output_path, "resolved virtual camera path", 4096U, true);

    if (!processing_runtime::supports_algorithm_settings(
            document.stream.algorithm
        )) {
        fail("configured processing algorithm is unavailable in this build");
    }

    if (const auto existing_stream = manager.find_stream(stream_name)) {
        if (existing_stream->get_path() != source) {
            fail(
                "stream '" + stream_name
                + "' already exists with a different source"
            );
        }
    }

    const auto stream_names = manager.stream_names();
    for (const configured_line& line_value : document.lines) {
        const auto existing_line = manager.find_line(line_value.name);
        if (!existing_line) {
            continue;
        }
        const bool used_by_other_stream = std::ranges::any_of(
            stream_names, [&](const std::string& candidate_stream) {
                if (candidate_stream == stream_name) {
                    return false;
                }
                const auto connected = manager.stream_lines(candidate_stream);
                return std::ranges::find(connected, line_value.name)
                    != connected.end();
            }
        );
        if (used_by_other_stream
            && (!same_geometry(*existing_line, line_value)
                || existing_line->dir != line_value.direction)) {
            fail(
                "line '" + line_value.name
                + "' is shared by another stream with different geometry "
                  "or direction"
            );
        }
    }
    if (!manager.find_stream(stream_name)) {
        manager.add_stream(source, stream_name, source_type, loop);
    }
    if (!runtime.set_stream_algorithm_settings(
            stream_name, document.stream.algorithm
        )) {
        fail(
            "cannot apply algorithm settings for stream '" + stream_name + "'"
        );
    }
    manager.set_stream_analysis_interval_ms(
        stream_name, document.stream.analysis_interval_ms
    );
    runtime.set_stream_processing_max_pixels(
        stream_name,
        document.stream.manual_processing_policy_enabled
            ? std::optional<int> { document.stream.manual_processing_pixels }
            : std::optional<int> {}
    );

    for (const std::string& connected_line :
         manager.stream_lines(stream_name)) {
        manager.clear_stream_line(stream_name, connected_line);
    }

    std::size_t connected = 0U;
    for (const configured_line& line_value : document.lines) {
        const line_ptr added = manager.upsert_line(
            line_value.points, line_value.closed, line_value.name
        );
        if (!added || added->name != line_value.name) {
            fail(
                "cannot preserve imported line name '" + line_value.name + "'"
            );
        }
        manager.set_line_profile(line_value.profile);
        manager.set_line_dir(line_value.name, line_value.direction);
        manager.set_stream_line_profile(stream_name, line_value.profile);
        if (line_value.enabled) {
            manager.set_line(stream_name, line_value.name);
            ++connected;
        }
    }

    return line_configuration_apply_result {
        .stream_name = stream_name,
        .source = source,
        .virtual_camera_path = output_path,
        .connected_line_count = connected,
    };
}

} // namespace yodau::core
