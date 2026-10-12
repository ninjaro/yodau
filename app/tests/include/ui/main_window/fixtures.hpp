#ifndef YODAU_APP_TESTS_UI_MAIN_WINDOW_FIXTURES_HPP
#define YODAU_APP_TESTS_UI_MAIN_WINDOW_FIXTURES_HPP

#include "shell/app_log.hpp"
#include "widgets/stream_cell.hpp"

#include <utility>
#include <vector>

namespace main_window_tests_support {

app_log_entry make_log_entry(
    const QDateTime& timestamp, app_log_area area, app_log_severity severity,
    const QString& subsystem, const QString& message,
    const QString& stream_name = QString(), const QString& detail = QString(),
    const QString& algorithm_id = QString(),
    const QString& line_name = QString(), const QString& event_type = QString(),
    const QColor& line_color = QColor()
);

QImage render_events(const stream_settings& settings_value);
QImage
render_status(const stream_settings& settings_value, app_log_mode log_mode);
QImage render_wave(const stream_cell::line_instance& line_value);
QImage gradient_frame(const QSize& size);
QImage render_frame(
    const QSize& cell_size, const QImage& frame,
    const std::vector<stream_cell::line_instance>& lines
);
QImage render_highlights(
    const QSize& cell_size, const QImage& frame,
    const std::vector<stream_cell::line_instance>& lines,
    const std::vector<std::pair<QString, QPointF>>& highlights
);
bool color_close(
    const QColor& actual, const QColor& expected, int tolerance = 4
);
bool point_close(
    const QPointF& actual, const QPointF& expected, double tolerance = 0.15
);
int pixel_difference(const QImage& lhs, const QImage& rhs);

} // namespace main_window_tests_support

#endif
