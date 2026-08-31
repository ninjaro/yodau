#ifndef YODAU_CORE_CONFIGURATION_LINE_CONFIGURATION_JSON_HPP
#define YODAU_CORE_CONFIGURATION_LINE_CONFIGURATION_JSON_HPP

#include "configuration/line_configuration_model.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace yodau::core {

inline constexpr std::string_view line_configuration_format {
    "yodau-line-configuration"
};
inline constexpr std::size_t maximum_line_configuration_bytes {
    4U * 1024U * 1024U
};

[[nodiscard]] std::string
encode_line_configuration_json(
    const line_configuration_document& document
);
[[nodiscard]] line_configuration_document
decode_line_configuration_json(std::string_view contents);

} // namespace yodau::core

#endif // YODAU_CORE_CONFIGURATION_LINE_CONFIGURATION_JSON_HPP
