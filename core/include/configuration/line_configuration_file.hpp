#ifndef YODAU_CORE_CONFIGURATION_LINE_CONFIGURATION_FILE_HPP
#define YODAU_CORE_CONFIGURATION_LINE_CONFIGURATION_FILE_HPP

#include "configuration/line_configuration_model.hpp"

#include <filesystem>

namespace yodau::data {

[[nodiscard]] yodau::core::line_configuration_document
load_line_configuration_file(const std::filesystem::path& path);
void save_line_configuration_file_atomic(
    const yodau::core::line_configuration_document& document,
    const std::filesystem::path& path
);

} // namespace yodau::data

#endif // YODAU_CORE_CONFIGURATION_LINE_CONFIGURATION_FILE_HPP
