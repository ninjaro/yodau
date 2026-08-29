#include "configuration/line_configuration.hpp"

#include "analysis/processing_runtime.hpp"
#include "streams/stream_manager.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <ranges>
#include <system_error>
#include <unordered_set>

#ifdef __unix__
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace yodau::core {
namespace {

    using json = nlohmann::json;

    constexpr size_t maximum_configuration_bytes { 4U * 1024U * 1024U };
    constexpr size_t maximum_lines { 4096U };
    constexpr size_t maximum_points_per_line { 16384U };
    constexpr size_t maximum_total_points { 100000U };

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

    bool has_control_character(const std::string_view text) {
        return std::ranges::any_of(text, [](const unsigned char value) {
            return value < 0x20U || value == 0x7fU;
        });
    }

    void validate_text(
        const std::string& value, const std::string& location,
        const size_t maximum_size, const bool allow_empty = false
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

    bool valid_color(const std::string& value) {
        if (value.size() != 7U && value.size() != 9U) {
            return false;
        }
        if (value.front() != '#') {
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
                fail(
                    location + " contains an unsupported value '" + *text + "'"
                );
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

    void validate_canonical_name(
        const std::string& value, const std::string& location
    ) {
        if (!value.empty()
            && (std::isspace(static_cast<unsigned char>(value.front())) != 0
                || std::isspace(static_cast<unsigned char>(value.back()))
                    != 0)) {
            fail(location + " must not have leading or trailing whitespace");
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

    bool
    same_geometry(const line& existing, const configured_line& configured) {
        if (existing.closed != configured.closed
            || existing.points.size() != configured.points.size()) {
            return false;
        }
        for (size_t index = 0; index < existing.points.size(); ++index) {
            if (!existing.points[index].compare(configured.points[index])) {
                return false;
            }
        }
        return true;
    }

    std::filesystem::path
    temporary_path_for(const std::filesystem::path& target) {
        static std::atomic_uint64_t sequence { 0U };
        const auto tick
            = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto suffix = sequence.fetch_add(1U, std::memory_order_relaxed);
        return target.parent_path()
            / (target.filename().string() + ".tmp." + std::to_string(tick) + "."
               + std::to_string(suffix));
    }

#ifdef __unix__
    void write_private_file(
        const std::filesystem::path& path, const std::string_view contents
    ) {
        const int fd = ::open(
            path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
            S_IRUSR | S_IWUSR
        );
        if (fd < 0) {
            fail(
                "cannot create private temporary line configuration '"
                + path.string() + "': " + std::strerror(errno)
            );
        }

        size_t offset = 0U;
        while (offset < contents.size()) {
            const ssize_t written = ::write(
                fd, contents.data() + offset, contents.size() - offset
            );
            if (written < 0 && errno == EINTR) {
                continue;
            }
            if (written <= 0) {
                const int write_error = errno;
                ::close(fd);
                fail(
                    "cannot write temporary line configuration '"
                    + path.string() + "': " + std::strerror(write_error)
                );
            }
            offset += static_cast<size_t>(written);
        }

        if (::fsync(fd) < 0) {
            const int sync_error = errno;
            ::close(fd);
            fail(
                "cannot sync temporary line configuration '" + path.string()
                + "': " + std::strerror(sync_error)
            );
        }
        if (::close(fd) < 0) {
            fail(
                "cannot close temporary line configuration '" + path.string()
                + "': " + std::strerror(errno)
            );
        }
    }

    void sync_parent_directory(const std::filesystem::path& target) {
        const std::filesystem::path parent = target.parent_path().empty()
            ? std::filesystem::path(".")
            : target.parent_path();
        const int fd
            = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (fd < 0) {
            fail(
                "cannot open line configuration directory '" + parent.string()
                + "' for sync: " + std::strerror(errno)
            );
        }
        const int result = ::fsync(fd);
        const int sync_error = errno;
        ::close(fd);
        if (result < 0) {
            fail(
                "cannot sync line configuration directory '" + parent.string()
                + "': " + std::strerror(sync_error)
            );
        }
    }
#endif

    class json_line_configuration_codec final
        : public line_configuration_codec {
    public:
        [[nodiscard]] std::string
        encode(const line_configuration_document& document) const override;
        [[nodiscard]] line_configuration_document
        decode(std::string_view contents) const override;
    };

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
    for (size_t index = 0; index < points.size(); ++index) {
        if (!points[index].compare(other.points[index])) {
            return false;
        }
    }
    return true;
}

void validate_line_configuration(const line_configuration_document& document) {
    if (document.version != current_line_configuration_version) {
        fail(
            "unsupported line configuration version "
            + std::to_string(document.version)
        );
    }

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
    size_t total_points = 0U;
    for (size_t index = 0; index < document.lines.size(); ++index) {
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

std::string json_line_configuration_codec::encode(
    const line_configuration_document& document
) const {
    validate_line_configuration(document);
    json lines = json::array();
    for (const configured_line& line_value : document.lines) {
        lines.push_back(configured_line_json(line_value));
    }
    const json root {
        { "format", line_configuration_format },
        { "version", document.version },
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
json_line_configuration_codec::decode(const std::string_view contents) const {
    if (contents.empty()) {
        fail("line configuration is empty");
    }
    if (contents.size() > maximum_configuration_bytes) {
        fail("line configuration exceeds the 4 MiB size limit");
    }

    try {
        const json root = json::parse(contents.begin(), contents.end());
        require_exact_keys(
            root, { "format", "version", "stream", "lines" }, "root"
        );
        if (required_value<std::string>(root, "format", "root")
            != line_configuration_format) {
            fail("root.format is not a yodau line configuration");
        }

        line_configuration_document document;
        document.version = required_value<int>(root, "version", "root");
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

const line_configuration_codec& line_configuration_json_codec() noexcept {
    static const json_line_configuration_codec codec;
    return codec;
}

std::string
serialize_line_configuration(const line_configuration_document& document) {
    return serialize_line_configuration(
        document, line_configuration_json_codec()
    );
}

std::string serialize_line_configuration(
    const line_configuration_document& document,
    const line_configuration_codec& codec
) {
    return codec.encode(document);
}

line_configuration_document
parse_line_configuration(const std::string_view contents) {
    return parse_line_configuration(contents, line_configuration_json_codec());
}

line_configuration_document parse_line_configuration(
    const std::string_view contents, const line_configuration_codec& codec
) {
    return codec.decode(contents);
}

line_configuration_document
load_line_configuration(const std::filesystem::path& path) {
    return load_line_configuration(path, line_configuration_json_codec());
}

line_configuration_document load_line_configuration(
    const std::filesystem::path& path, const line_configuration_codec& codec
) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        fail("cannot open line configuration '" + path.string() + "'");
    }
    std::string contents;
    contents.reserve(
        std::min<size_t>(maximum_configuration_bytes, 64U * 1024U)
    );
    std::array<char, 64U * 1024U> buffer {};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto bytes = input.gcount();
        if (bytes > 0) {
            const auto count = static_cast<size_t>(bytes);
            if (contents.size() > maximum_configuration_bytes - count) {
                fail("line configuration exceeds the 4 MiB size limit");
            }
            contents.append(buffer.data(), count);
        }
    }
    if (!input.eof()) {
        fail("cannot read line configuration '" + path.string() + "'");
    }
    return codec.decode(contents);
}

void save_line_configuration_atomic(
    const line_configuration_document& document,
    const std::filesystem::path& path
) {
    save_line_configuration_atomic(
        document, path, line_configuration_json_codec()
    );
}

void save_line_configuration_atomic(
    const line_configuration_document& document,
    const std::filesystem::path& path, const line_configuration_codec& codec
) {
    if (path.empty() || path.filename().empty()) {
        fail("line configuration output path must name a file");
    }
    const std::string contents = codec.encode(document);
    const std::filesystem::path temporary = temporary_path_for(path);

    try {
#ifdef __unix__
        write_private_file(temporary, contents);
        if (::rename(temporary.c_str(), path.c_str()) < 0) {
            fail(
                "cannot replace line configuration '" + path.string()
                + "': " + std::strerror(errno)
            );
        }
        sync_parent_directory(path);
#else
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output) {
                fail(
                    "cannot create temporary line configuration '"
                    + temporary.string() + "'"
                );
            }
            output.write(
                contents.data(), static_cast<std::streamsize>(contents.size())
            );
            output.flush();
            if (!output) {
                fail(
                    "cannot write temporary line configuration '"
                    + temporary.string() + "'"
                );
            }
        }

        std::filesystem::rename(temporary, path);
#endif
    } catch (const line_configuration_error&) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    } catch (const std::filesystem::filesystem_error& error) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        fail(
            "cannot replace line configuration '" + path.string()
            + "': " + error.code().message()
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
    if (source_type != "local" && source_type != "file" && source_type != "rtsp"
        && source_type != "http") {
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

    size_t connected = 0U;
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
