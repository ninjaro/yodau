#include "widgets/stream_cell.hpp"

#include <algorithm>
#include <cmath>

namespace stream_cell_support {

QPointF average_point_pct(const std::vector<QPointF>& pts_pct) {
    if (pts_pct.empty()) {
        return {};
    }

    double sum_x = 0.0;
    double sum_y = 0.0;
    for (const QPointF& pt_pct : pts_pct) {
        sum_x += pt_pct.x();
        sum_y += pt_pct.y();
    }

    return { sum_x / static_cast<double>(pts_pct.size()),
             sum_y / static_cast<double>(pts_pct.size()) };
}

QColor
sampled_stream_color(const QImage& frame, const std::vector<QPointF>& pts_pct) {
    if (frame.isNull() || pts_pct.empty()) {
        return {};
    }

    const QPointF anchor_pct = average_point_pct(pts_pct);
    const int center_x = std::clamp(
        static_cast<int>(
            std::lround(anchor_pct.x() / 100.0 * std::max(0, frame.width() - 1))
        ),
        0, std::max(0, frame.width() - 1)
    );
    const int center_y = std::clamp(
        static_cast<int>(std::lround(
            anchor_pct.y() / 100.0 * std::max(0, frame.height() - 1)
        )),
        0, std::max(0, frame.height() - 1)
    );

    int sample_count = 0;
    int sum_red = 0;
    int sum_green = 0;
    int sum_blue = 0;

    for (int dy = -1; dy <= 1; dy += 1) {
        for (int dx = -1; dx <= 1; dx += 1) {
            const int sample_x
                = std::clamp(center_x + dx, 0, frame.width() - 1);
            const int sample_y
                = std::clamp(center_y + dy, 0, frame.height() - 1);
            const QColor sample = frame.pixelColor(sample_x, sample_y);
            sum_red += sample.red();
            sum_green += sample.green();
            sum_blue += sample.blue();
            sample_count += 1;
        }
    }

    if (sample_count <= 0) {
        return {};
    }

    return { sum_red / sample_count, sum_green / sample_count,
             sum_blue / sample_count };
}

} // namespace stream_cell_support

QColor stream_cell::color_with_alpha(QColor color, const int alpha) {
    color.setAlpha(std::clamp(alpha, 0, 255));
    return color;
}

QColor stream_cell::contrasting_text_color(const QColor& background) {
    const double luma = 0.2126 * background.redF()
        + 0.7152 * background.greenF() + 0.0722 * background.blueF();
    return luma >= 0.62 ? QColor(Qt::black) : QColor(Qt::white);
}

QColor stream_cell::line_color_for(
    const stream_cell::line_instance& line_value, const int line_index,
    const int line_count, const QImage& frame
) {
    const QString color_mode_id
        = normalized_line_color_mode_id(line_value.color_mode_id);

    if (color_mode_id == QStringLiteral("manual")) {
        return line_value.color.isValid() ? line_value.color : QColor(Qt::red);
    }

    if (color_mode_id == QStringLiteral("negative_auto")) {
        const QColor sampled = stream_cell_support::sampled_stream_color(
            frame, line_value.pts_pct
        );
        if (sampled.isValid()) {
            return softened_negative_line_color(sampled);
        }
    }

    return auto_palette_line_color(line_index, line_count);
}

QColor stream_cell::draft_color_for(
    const QString& color_mode_id, const QColor& manual_color,
    const std::vector<QPointF>& pts_pct, const QImage& frame,
    const int line_index, const int line_count
) {
    stream_cell::line_instance line_value;
    line_value.color = manual_color;
    line_value.color_mode_id = color_mode_id;
    line_value.pts_pct = pts_pct;
    return line_color_for(line_value, line_index, line_count, frame);
}

int stream_cell::enabled_line_count(
    const std::vector<stream_cell::line_instance>& lines
) {
    return static_cast<int>(std::count_if(
        lines.cbegin(), lines.cend(),
        [](const stream_cell::line_instance& line_value) {
            return line_value.enabled;
        }
    ));
}
