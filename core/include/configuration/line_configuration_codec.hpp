#ifndef YODAU_CORE_CONFIGURATION_LINE_CONFIGURATION_CODEC_HPP
#define YODAU_CORE_CONFIGURATION_LINE_CONFIGURATION_CODEC_HPP

#include "configuration/line_configuration_model.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace yodau::core {

inline constexpr std::string_view line_configuration_format {
    "yodau-line-configuration"
};

// Encoding is deliberately expressed only in terms of the portable domain
// model and byte text. JSON implementation types must not cross this boundary.
// The application currently ships one codec; this interface is the narrow
// extension point for a future format only if a real compatibility need arises.
class line_configuration_codec {
public:
    virtual ~line_configuration_codec() = default;

    [[nodiscard]] virtual std::string
    encode(const line_configuration_document& document) const = 0;
    [[nodiscard]] virtual line_configuration_document
    decode(std::string_view contents) const = 0;
};

[[nodiscard]] const line_configuration_codec&
line_configuration_json_codec() noexcept;

// Compatibility entry points retain the established JSON API and file format.
[[nodiscard]] std::string
serialize_line_configuration(const line_configuration_document& document);
[[nodiscard]] line_configuration_document
parse_line_configuration(std::string_view contents);
[[nodiscard]] line_configuration_document
load_line_configuration(const std::filesystem::path& path);
void save_line_configuration_atomic(
    const line_configuration_document& document,
    const std::filesystem::path& path
);

// Explicit-codec overloads keep persistence policy at the storage boundary
// without introducing another format or coupling the model to a JSON library.
[[nodiscard]] std::string serialize_line_configuration(
    const line_configuration_document& document,
    const line_configuration_codec& codec
);
[[nodiscard]] line_configuration_document parse_line_configuration(
    std::string_view contents, const line_configuration_codec& codec
);
[[nodiscard]] line_configuration_document load_line_configuration(
    const std::filesystem::path& path, const line_configuration_codec& codec
);
void save_line_configuration_atomic(
    const line_configuration_document& document,
    const std::filesystem::path& path, const line_configuration_codec& codec
);

} // namespace yodau::core

#endif // YODAU_CORE_CONFIGURATION_LINE_CONFIGURATION_CODEC_HPP
