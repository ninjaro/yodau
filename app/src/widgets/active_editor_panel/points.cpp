#include "widgets/active_editor_panel.hpp"

#include "widgets/active_editor_panel/line_lookup.hpp"
#include "widgets/active_editor_panel/point_rows.hpp"

#include <QLineF>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace active_editor_panel_support {

QPointF clamped_shape_delta_pct(
    const std::vector<QPointF>& points_pct, const std::vector<int>& rows,
    const QPointF& requested_delta_pct
) {
    if (rows.empty()) {
        return {};
    }

    double min_x = std::numeric_limits<double>::max();
    double max_x = std::numeric_limits<double>::lowest();
    double min_y = std::numeric_limits<double>::max();
    double max_y = std::numeric_limits<double>::lowest();

    for (const int row : rows) {
        const QPointF point
            = points_pct.at(static_cast<std::vector<QPointF>::size_type>(row));
        min_x = std::min(min_x, point.x());
        max_x = std::max(max_x, point.x());
        min_y = std::min(min_y, point.y());
        max_y = std::max(max_y, point.y());
    }

    return {
        std::clamp(requested_delta_pct.x(), -min_x, 100.0 - max_x),
        std::clamp(requested_delta_pct.y(), -min_y, 100.0 - max_y),
    };
}

QPointF average_point_pct(
    const std::vector<QPointF>& points_pct, const std::vector<int>& rows
) {
    if (rows.empty()) {
        return {};
    }

    QPointF sum;
    for (const int row : rows) {
        sum += points_pct.at(static_cast<std::vector<QPointF>::size_type>(row));
    }

    return sum / static_cast<double>(rows.size());
}

QPointF wire_centroid_pct(
    const std::vector<QPointF>& points_pct, const std::vector<int>& rows,
    const bool closed
) {
    if (rows.empty()) {
        return {};
    }
    if (rows.size() == 1) {
        return points_pct.at(
            static_cast<std::vector<QPointF>::size_type>(rows.front())
        );
    }

    QPointF weighted_sum;
    double total_weight = 0.0;

    auto accumulate_segment = [&](const QPointF& a, const QPointF& b) {
        const QLineF segment(a, b);
        const double length = segment.length();
        if (length <= std::numeric_limits<double>::epsilon()) {
            return;
        }
        weighted_sum += (a + b) / 2.0 * length;
        total_weight += length;
    };

    for (int index = 1; index < static_cast<int>(rows.size()); index += 1) {
        const auto previous_row_index
            = static_cast<std::vector<int>::size_type>(index - 1);
        const auto current_row_index
            = static_cast<std::vector<int>::size_type>(index);
        const QPointF a = points_pct.at(
            static_cast<std::vector<QPointF>::size_type>(
                rows.at(previous_row_index)
            )
        );
        const QPointF b = points_pct.at(
            static_cast<std::vector<QPointF>::size_type>(
                rows.at(current_row_index)
            )
        );
        accumulate_segment(a, b);
    }

    if (closed && rows.size() >= 3) {
        const QPointF a = points_pct.at(
            static_cast<std::vector<QPointF>::size_type>(rows.back())
        );
        const QPointF b = points_pct.at(
            static_cast<std::vector<QPointF>::size_type>(rows.front())
        );
        accumulate_segment(a, b);
    }

    if (total_weight <= std::numeric_limits<double>::epsilon()) {
        return average_point_pct(points_pct, rows);
    }

    return weighted_sum / total_weight;
}

QPointF rotated_point_pct(
    const QPointF& point_pct, const QPointF& pivot_pct, const double radians
) {
    const QPointF relative = point_pct - pivot_pct;
    const double sin_value = std::sin(radians);
    const double cos_value = std::cos(radians);
    return {
        pivot_pct.x() + relative.x() * cos_value - relative.y() * sin_value,
        pivot_pct.y() + relative.x() * sin_value + relative.y() * cos_value
    };
}

} // namespace active_editor_panel_support

bool active_editor_panel::select_line_edit_point(const int visible_index) {
    const auto selected_row
        = active_editor_panel_support::row_for_visible_index(
            line_edit_point_enabled_, visible_index
        );
    if (!selected_row.has_value()) {
        return false;
    }

    line_edit_selected_row_ = *selected_row;
    refresh_line_edit_table();
    return true;
}

bool active_editor_panel::translate_line_edit_shape(const QPointF& delta_pct) {
    const auto visible_rows = active_editor_panel_support::enabled_row_indices(
        line_edit_point_enabled_
    );
    if (line_edit_source_name_.isEmpty() || visible_rows.empty()
        || line_edit_points_pct_.size() != line_edit_point_enabled_.size()) {
        return false;
    }

    const QPointF adjusted_delta
        = active_editor_panel_support::clamped_shape_delta_pct(
            line_edit_points_pct_, visible_rows, delta_pct
        );
    if (std::abs(adjusted_delta.x()) <= std::numeric_limits<double>::epsilon()
        && std::abs(adjusted_delta.y())
            <= std::numeric_limits<double>::epsilon()) {
        return false;
    }

    record_edit();
    for (const int row : visible_rows) {
        line_edit_points_pct_.at(
            static_cast<std::vector<QPointF>::size_type>(row)
        ) += adjusted_delta;
    }

    refresh_edit_view();
    return true;
}

bool active_editor_panel::move_line_edit_point(
    const int visible_index, const QPointF& point_pct
) {
    const auto selected_row
        = active_editor_panel_support::row_for_visible_index(
            line_edit_point_enabled_, visible_index
        );
    if (!selected_row.has_value() || line_edit_source_name_.isEmpty()
        || *selected_row >= static_cast<int>(line_edit_points_pct_.size())) {
        return false;
    }

    const QPointF clamped_point
        = active_editor_panel_support::clamped_pct(point_pct);
    const QPointF current_point = line_edit_points_pct_.at(
        static_cast<std::vector<QPointF>::size_type>(*selected_row)
    );
    if (std::abs(current_point.x() - clamped_point.x())
            <= std::numeric_limits<double>::epsilon()
        && std::abs(current_point.y() - clamped_point.y())
            <= std::numeric_limits<double>::epsilon()) {
        return false;
    }

    record_edit();
    line_edit_selected_row_ = *selected_row;
    line_edit_points_pct_.at(
        static_cast<std::vector<QPointF>::size_type>(*selected_row)
    ) = clamped_point;

    refresh_edit_view();
    return true;
}

bool active_editor_panel::split_line_edit_point(const int visible_index) {
    const auto visible_rows = active_editor_panel_support::enabled_row_indices(
        line_edit_point_enabled_
    );
    if (line_edit_source_name_.isEmpty() || visible_rows.size() < 2
        || visible_index < 0
        || visible_index >= static_cast<int>(visible_rows.size())) {
        return false;
    }

    const int row = visible_rows.at(
        static_cast<std::vector<int>::size_type>(visible_index)
    );
    if (row < 0 || row >= static_cast<int>(line_edit_points_pct_.size())) {
        return false;
    }

    const QPointF current = line_edit_points_pct_.at(
        static_cast<std::vector<QPointF>::size_type>(row)
    );

    QPointF first_split = current;
    QPointF second_split = current;

    if (visible_index == 0) {
        const QPointF next = line_edit_points_pct_.at(
            static_cast<std::vector<QPointF>::size_type>(visible_rows.at(1))
        );
        first_split = current + (next - current) / 3.0;
        second_split = current + (next - current) * (2.0 / 3.0);
    } else if (visible_index == static_cast<int>(visible_rows.size()) - 1) {
        const QPointF previous = line_edit_points_pct_.at(
            static_cast<std::vector<QPointF>::size_type>(visible_rows.at(
                static_cast<std::vector<int>::size_type>(visible_index - 1)
            ))
        );
        first_split = previous + (current - previous) / 3.0;
        second_split = previous + (current - previous) * (2.0 / 3.0);
    } else {
        const QPointF previous = line_edit_points_pct_.at(
            static_cast<std::vector<QPointF>::size_type>(visible_rows.at(
                static_cast<std::vector<int>::size_type>(visible_index - 1)
            ))
        );
        const QPointF next = line_edit_points_pct_.at(
            static_cast<std::vector<QPointF>::size_type>(visible_rows.at(
                static_cast<std::vector<int>::size_type>(visible_index) + 1U
            ))
        );
        first_split = previous + (current - previous) / 2.0;
        second_split = current + (next - current) / 2.0;
    }

    record_edit();
    line_edit_selected_row_ = row;
    line_edit_points_pct_.at(static_cast<std::vector<QPointF>::size_type>(row))
        = active_editor_panel_support::clamped_pct(first_split);
    line_edit_points_pct_.insert(
        line_edit_points_pct_.begin() + row + 1,
        active_editor_panel_support::clamped_pct(second_split)
    );
    line_edit_point_enabled_.insert(
        line_edit_point_enabled_.begin() + row + 1, true
    );

    refresh_edit_view();
    return true;
}

bool active_editor_panel::insert_line_edit_point_after(
    const int visible_segment_index, const QPointF& point_pct
) {
    const auto visible_rows = active_editor_panel_support::enabled_row_indices(
        line_edit_point_enabled_
    );
    const auto source_line = active_editor_panel_support::find_line(
        active_lines_, line_edit_source_name_
    );
    if (!source_line.has_value() || visible_rows.size() < 2
        || line_edit_points_pct_.size() != line_edit_point_enabled_.size()) {
        return false;
    }

    const int segment_count = source_line->closed && visible_rows.size() >= 3
        ? static_cast<int>(visible_rows.size())
        : static_cast<int>(visible_rows.size()) - 1;
    if (visible_segment_index < 0 || visible_segment_index >= segment_count) {
        return false;
    }

    const int segment_start_row = visible_rows.at(
        static_cast<std::vector<int>::size_type>(visible_segment_index)
    );
    const int insert_row = segment_start_row + 1;
    if (insert_row < 0
        || insert_row > static_cast<int>(line_edit_points_pct_.size())) {
        return false;
    }

    record_edit();
    line_edit_points_pct_.insert(
        line_edit_points_pct_.begin() + insert_row,
        active_editor_panel_support::clamped_pct(point_pct)
    );
    line_edit_point_enabled_.insert(
        line_edit_point_enabled_.begin() + insert_row, true
    );
    line_edit_selected_row_ = insert_row;
    refresh_edit_view();
    return true;
}

bool active_editor_panel::delete_line_edit_row(const int row) {
    if (line_edit_source_name_.isEmpty() || row < 0
        || row >= static_cast<int>(line_edit_points_pct_.size())
        || line_edit_points_pct_.size() != line_edit_point_enabled_.size()) {
        return false;
    }

    const bool row_is_enabled = line_edit_point_enabled_.at(
        static_cast<std::vector<bool>::size_type>(row)
    );
    const int kept_points = static_cast<int>(std::count(
        line_edit_point_enabled_.cbegin(), line_edit_point_enabled_.cend(), true
    ));
    if (row_is_enabled && kept_points <= 2) {
        return false;
    }

    record_edit();
    line_edit_points_pct_.erase(line_edit_points_pct_.begin() + row);
    line_edit_point_enabled_.erase(line_edit_point_enabled_.begin() + row);
    if (line_edit_points_pct_.empty()) {
        line_edit_selected_row_ = -1;
    } else {
        line_edit_selected_row_
            = std::min(row, static_cast<int>(line_edit_points_pct_.size()) - 1);
    }
    refresh_edit_view();
    return true;
}

bool active_editor_panel::delete_line_edit_point(const int visible_index) {
    const auto selected_row
        = active_editor_panel_support::row_for_visible_index(
            line_edit_point_enabled_, visible_index
        );
    return selected_row.has_value() && delete_line_edit_row(*selected_row);
}

bool active_editor_panel::rotate_line_edit_shape(
    const double delta_degrees, const int visible_pivot_index
) {
    const auto visible_rows = active_editor_panel_support::enabled_row_indices(
        line_edit_point_enabled_
    );
    if (line_edit_source_name_.isEmpty() || visible_rows.size() < 2
        || std::abs(delta_degrees) <= std::numeric_limits<double>::epsilon()
        || line_edit_points_pct_.size() != line_edit_point_enabled_.size()) {
        return false;
    }

    const auto source_line = active_editor_panel_support::find_line(
        active_lines_, line_edit_source_name_
    );
    if (!source_line.has_value()) {
        return false;
    }

    QPointF pivot_pct;
    if (visible_pivot_index >= 0) {
        const auto pivot_row
            = active_editor_panel_support::row_for_visible_index(
                line_edit_point_enabled_, visible_pivot_index
            );
        if (!pivot_row.has_value()) {
            return false;
        }
        line_edit_selected_row_ = *pivot_row;
        pivot_pct = line_edit_points_pct_.at(
            static_cast<std::vector<QPointF>::size_type>(*pivot_row)
        );
    } else {
        pivot_pct = active_editor_panel_support::wire_centroid_pct(
            line_edit_points_pct_, visible_rows, source_line->closed
        );
    }

    const double radians = delta_degrees * std::numbers::pi / 180.0;
    record_edit();
    for (const int row : visible_rows) {
        const auto row_index
            = static_cast<std::vector<QPointF>::size_type>(row);
        line_edit_points_pct_.at(row_index)
            = active_editor_panel_support::clamped_pct(
                active_editor_panel_support::rotated_point_pct(
                    line_edit_points_pct_.at(row_index), pivot_pct, radians
                )
            );
    }

    refresh_edit_view();
    return true;
}
