#ifndef YODAU_APP_WIDGETS_LINE_WAVE_RENDERER_PAINT_HPP
#define YODAU_APP_WIDGETS_LINE_WAVE_RENDERER_PAINT_HPP

#include "widgets/line_wave_renderer/pulse.hpp"

#include <QColor>
#include <QLineF>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QSize>

namespace line_wave_renderer {

inline QPointF to_px(const QPointF& pos_pct, const QSize& target_size) {
    return { pos_pct.x() / 100.0 * static_cast<double>(target_size.width()),
             pos_pct.y() / 100.0 * static_cast<double>(target_size.height()) };
}

inline bool draw(
    QPainter& painter, const path_geometry& geometry,
    const QVector<pulse>& pulses, const QSize& target_size,
    const QColor& line_color, const QString& width_text,
    const QString& length_text, const QString& response_text,
    const qint64 now_ms, const double display_gain = 1.0
) {
    if (pulses.isEmpty() || geometry.segments.empty()) {
        return false;
    }

    const auto style = style_for_line(width_text, length_text, response_text);
    // Presentation only: never change pulse dynamics or the default live wave.
    const double gain = std::isfinite(display_gain)
        ? std::clamp(display_gain, 1.0, 3.0)
        : 1.0;
    QPolygonF wave_poly;
    bool has_visible_wave = false;

    for (size_t segment_i = 0; segment_i < geometry.segments.size();
         ++segment_i) {
        const auto& segment = geometry.segments[segment_i];
        const QLineF segment_px(
            to_px(segment.a_pct, target_size), to_px(segment.b_pct, target_size)
        );
        const double segment_px_len = segment_px.length();
        if (segment_px_len <= 0.0) {
            continue;
        }

        const QPointF normal_px(
            -segment_px.dy() / segment_px_len, segment_px.dx() / segment_px_len
        );
        const int samples = std::clamp(
            static_cast<int>(std::lround(segment.length * 0.8)), 6, 28
        );

        for (int sample_i = segment_i == 0 ? 0 : 1; sample_i <= samples;
             sample_i += 1) {
            const double t
                = static_cast<double>(sample_i) / static_cast<double>(samples);
            const QPointF base_pct(
                segment.a_pct.x() + (segment.b_pct.x() - segment.a_pct.x()) * t,
                segment.a_pct.y() + (segment.b_pct.y() - segment.a_pct.y()) * t
            );
            const QPointF base_px = to_px(base_pct, target_size);
            const double sample_path_pos
                = segment.start_pos + t * segment.length;
            const double sample_displacement_px = gain
                * displacement_px(geometry, segment.index, sample_path_pos,
                                  now_ms, pulses);

            if (std::abs(sample_displacement_px) > 0.05) {
                has_visible_wave = true;
            }

            wave_poly << QPointF(
                base_px.x() + normal_px.x() * sample_displacement_px,
                base_px.y() + normal_px.y() * sample_displacement_px
            );
        }
    }

    if (!has_visible_wave || wave_poly.size() < 2) {
        return false;
    }

    QColor glow = line_color.lighter(160);
    glow.setAlpha(
        std::clamp(static_cast<int>(110.0 * style.glow_width_k), 80, 180)
    );
    QPen glow_pen(glow);
    glow_pen.setWidthF(
        2.8 + line_width_visual_value(width_text) * 0.42 * style.glow_width_k
    );
    painter.setPen(glow_pen);
    painter.drawPolyline(wave_poly);

    QColor wave_color = line_color.lighter(125);
    wave_color.setAlpha(
        std::clamp(
            static_cast<int>(208.0 + (style.amplitude_k - 1.0) * 36.0), 160, 245
        )
    );
    QPen wave_pen(wave_color);
    wave_pen.setWidthF(
        std::max(
            1.8,
            line_width_visual_value(width_text)
                * (0.52 + (style.spread_k - 1.0) * 0.18)
        )
    );
    painter.setPen(wave_pen);
    painter.drawPolyline(wave_poly);
    return true;
}

} // namespace line_wave_renderer

#endif
