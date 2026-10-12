#ifndef YODAU_APP_WIDGETS_ACTIVE_EDITOR_PANEL_LINE_LOOKUP_HPP
#define YODAU_APP_WIDGETS_ACTIVE_EDITOR_PANEL_LINE_LOOKUP_HPP

#include "widgets/stream_cell.hpp"

#include <algorithm>
#include <optional>

namespace active_editor_panel_support {

inline QString variant_name(const QString& source_name) {
    const QString trimmed_name = source_name.trimmed();
    return trimmed_name.isEmpty() ? QString()
                                  : trimmed_name + QStringLiteral("_edit");
}

inline std::optional<stream_cell::line_instance> find_line(
    const std::vector<stream_cell::line_instance>& lines,
    const QString& line_name
) {
    const QString trimmed_name = line_name.trimmed();
    const auto it = std::find_if(
        lines.cbegin(), lines.cend(),
        [&trimmed_name](const stream_cell::line_instance& line_value) {
            return line_value.template_name.trimmed() == trimmed_name;
        }
    );
    if (it == lines.cend()) {
        return std::nullopt;
    }
    return *it;
}

} // namespace active_editor_panel_support

#endif
