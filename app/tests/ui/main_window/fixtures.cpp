#include "ui/main_window/fixtures.hpp"

#include <QVideoFrame>

#include <algorithm>
#include <cmath>

namespace main_window_tests_support {

app_log_entry make_log_entry(
    const QDateTime& timestamp, const app_log_area area,
    const app_log_severity severity, const QString& subsystem,
    const QString& message, const QString& stream_name, const QString& detail,
    const QString& algorithm_id, const QString& line_name,
    const QString& event_type, const QColor& line_color
) {
    app_log_entry entry;
    entry.timestamp = timestamp;
    entry.area = area;
    entry.severity = severity;
    entry.subsystem = subsystem;
    entry.stream_name = stream_name;
    entry.line_name = line_name;
    entry.algorithm_id = algorithm_id;
    entry.event_type = event_type;
    entry.message = message;
    entry.detail = detail;
    entry.line_color = line_color;
    return entry;
}

QImage render_events(const stream_settings& settings_value) {
    stream_cell cell(QStringLiteral("cam-render"));
    cell.resize(240, 180);
    cell.setStyleSheet(
        QStringLiteral("background-color: black; color: white;")
    );
    cell.set_stream_settings(settings_value);
    cell.add_event(QPointF(25.0, 25.0), QColor(QStringLiteral("#ff8844")));

    QImage image(cell.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    cell.render(&image);

    return image.copy(QRect(16, 12, 88, 68));
}

QImage render_status(
    const stream_settings& settings_value, const app_log_mode log_mode
) {
    stream_cell cell(QStringLiteral("cam-status"));
    cell.resize(240, 180);
    cell.setStyleSheet(
        QStringLiteral("background-color: black; color: white;")
    );
    cell.set_stream_settings(settings_value);
    cell.set_log_mode(log_mode);

    QImage image(cell.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    cell.render(&image);

    return image.copy(QRect(96, 118, 136, 54));
}

QImage render_wave(const stream_cell::line_instance& line_value) {
    stream_cell cell(QStringLiteral("cam-wave"));
    cell.resize(240, 180);
    cell.setStyleSheet(
        QStringLiteral("background-color: black; color: white;")
    );
    cell.set_persistent_lines({ line_value });
    cell.highlight_line_at(
        line_value.template_name, QPointF(50.0, 70.0), 1.0, QString(), 1.0
    );

    QImage image(cell.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    cell.render(&image);

    return image.copy(QRect(24, 98, 192, 56));
}

QImage gradient_frame(const QSize& size) {
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < size.height(); y += 1) {
        for (int x = 0; x < size.width(); x += 1) {
            image.setPixelColor(
                x, y,
                QColor(
                    std::clamp(x, 0, 255), std::clamp(y, 0, 255),
                    std::clamp((x + y) / 2, 0, 255)
                )
            );
        }
    }
    return image;
}

QImage render_frame(
    const QSize& cell_size, const QImage& frame,
    const std::vector<stream_cell::line_instance>& lines
) {
    stream_cell cell(QStringLiteral("cam-render"));
    cell.resize(cell_size);
    cell.setStyleSheet(
        QStringLiteral("background-color: black; color: white;")
    );
    cell.set_persistent_lines(lines);
    const bool invoked = QMetaObject::invokeMethod(
        &cell, "on_frame_changed", Qt::DirectConnection,
        Q_ARG(QVideoFrame, QVideoFrame(frame))
    );
    Q_ASSERT(invoked);
    Q_UNUSED(invoked);

    QImage image(cell.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    cell.render(&image);
    return image;
}

QImage render_highlights(
    const QSize& cell_size, const QImage& frame,
    const std::vector<stream_cell::line_instance>& lines,
    const std::vector<std::pair<QString, QPointF>>& highlights
) {
    stream_cell cell(QStringLiteral("cam-render"));
    cell.resize(cell_size);
    cell.setStyleSheet(
        QStringLiteral("background-color: black; color: white;")
    );
    cell.set_persistent_lines(lines);
    const bool invoked = QMetaObject::invokeMethod(
        &cell, "on_frame_changed", Qt::DirectConnection,
        Q_ARG(QVideoFrame, QVideoFrame(frame))
    );
    Q_ASSERT(invoked);
    Q_UNUSED(invoked);

    for (const auto& [line_name, pos_pct] : highlights) {
        cell.highlight_line_at(line_name, pos_pct);
    }

    QImage image(cell.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    cell.render(&image);
    return image;
}

bool color_close(
    const QColor& actual, const QColor& expected, const int tolerance
) {
    return std::abs(actual.red() - expected.red()) <= tolerance
        && std::abs(actual.green() - expected.green()) <= tolerance
        && std::abs(actual.blue() - expected.blue()) <= tolerance;
}

bool point_close(
    const QPointF& actual, const QPointF& expected, const double tolerance
) {
    return std::abs(actual.x() - expected.x()) <= tolerance
        && std::abs(actual.y() - expected.y()) <= tolerance;
}

int pixel_difference(const QImage& lhs, const QImage& rhs) {
    const int width = std::min(lhs.width(), rhs.width());
    const int height = std::min(lhs.height(), rhs.height());
    int diff_count = 0;

    for (int y = 0; y < height; y += 1) {
        for (int x = 0; x < width; x += 1) {
            if (lhs.pixelColor(x, y) != rhs.pixelColor(x, y)) {
                diff_count += 1;
            }
        }
    }

    return diff_count;
}

} // namespace main_window_tests_support
