#ifndef YODAU_APP_WIDGETS_ACTIVE_EDITOR_PANEL_POINT_ROWS_HPP
#define YODAU_APP_WIDGETS_ACTIVE_EDITOR_PANEL_POINT_ROWS_HPP

#include <QPointF>

#include <algorithm>
#include <optional>
#include <vector>

namespace active_editor_panel_support {

inline QPointF clamped_pct(QPointF point_pct) {
    point_pct.setX(std::clamp(point_pct.x(), 0.0, 100.0));
    point_pct.setY(std::clamp(point_pct.y(), 0.0, 100.0));
    return point_pct;
}

inline std::vector<int>
enabled_row_indices(const std::vector<bool>& enabled_rows) {
    std::vector<int> indices;
    indices.reserve(enabled_rows.size());

    for (int row = 0; row < static_cast<int>(enabled_rows.size()); row += 1) {
        if (enabled_rows.at(static_cast<std::vector<bool>::size_type>(row))) {
            indices.push_back(row);
        }
    }

    return indices;
}

inline std::optional<int> row_for_visible_index(
    const std::vector<bool>& enabled_rows, const int visible_index
) {
    const auto visible_rows = enabled_row_indices(enabled_rows);
    if (visible_index < 0
        || visible_index >= static_cast<int>(visible_rows.size())) {
        return std::nullopt;
    }

    return visible_rows.at(
        static_cast<std::vector<int>::size_type>(visible_index)
    );
}

inline std::optional<int>
visible_index_for_row(const std::vector<bool>& enabled_rows, const int row) {
    int visible_index = 0;
    for (int current_row = 0;
         current_row < static_cast<int>(enabled_rows.size());
         current_row += 1) {
        if (!enabled_rows.at(
                static_cast<std::vector<bool>::size_type>(current_row)
            )) {
            continue;
        }
        if (current_row == row) {
            return visible_index;
        }
        visible_index += 1;
    }

    return std::nullopt;
}

} // namespace active_editor_panel_support

#endif
