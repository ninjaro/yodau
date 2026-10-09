#include "widgets/stream_cell.hpp"

#include "widgets/processing_overlay_renderer.hpp"

#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <algorithm>
#include <cmath>

void stream_cell::draw_poly_with_points(
    QPainter& p, const std::vector<QPointF>& pts_pct, const QColor& color,
    bool closed, Qt::PenStyle style, qreal width, const bool draw_points
) const {
    if (pts_pct.size() < 2) {
        return;
    }

    QPen pen(color);
    pen.setWidthF(width);
    pen.setStyle(style);
    p.setPen(pen);

    QPolygonF poly;
    poly.reserve(static_cast<int>(pts_pct.size()));
    for (const auto& pt_pct : pts_pct) {
        poly << to_px(pt_pct);
    }

    if (closed && poly.size() >= 3) {
        p.drawPolygon(poly);
    } else {
        p.drawPolyline(poly);
    }

    if (draw_points) {
        for (const auto& pt_px : poly) {
            p.drawEllipse(pt_px, 3.0, 3.0);
        }
    }
}

void stream_cell::draw_persistent(QPainter& p) const {
    const auto now = QDateTime::currentDateTime();
    const int line_count = std::max(1, enabled_line_count(persistent_lines));
    int enabled_line_index = 0;

    for (int line_index = 0;
         line_index < static_cast<int>(persistent_lines.size());
         line_index += 1) {
        const auto& l = persistent_lines[static_cast<size_t>(line_index)];
        if (!l.enabled) {
            continue;
        }
        const auto key = l.template_name.trimmed();
        const QColor effective_color
            = line_color_for(l, enabled_line_index, line_count, last_frame);
        enabled_line_index += 1;

        if (!key.isEmpty() && line_highlights.contains(key)) {
            const int age = static_cast<int>(line_highlights[key].msecsTo(now));
            if (age < line_highlight_ttl_ms) {
                double ktime
                    = 1.0 - static_cast<double>(age) / line_highlight_ttl_ms;
                if (ktime < 0.0) {
                    ktime = 0.0;
                }

                const double falloff_pct = 30.0;
                const double base_w = line_width_visual_value(l.width_text);
                const double peak_w = base_w + 20.0;

                bool has_hit = false;
                QVector<hit_info> hit_points;

                if (line_hits.contains(key)) {
                    const auto hits = line_hits.value(key);
                    for (int i = 0; i < hits.size(); i += 1) {
                        const int hit_age
                            = static_cast<int>(hits[i].ts.msecsTo(now));
                        if (hit_age < line_highlight_ttl_ms) {
                            has_hit = true;
                            hit_points.push_back(hits[i]);
                        }
                    }
                }

                const double k_boost = 1.0;
                if (!has_hit) {
                    QColor hc = effective_color;
                    int a = static_cast<int>(255.0 * ktime);
                    if (a < 0) {
                        a = 0;
                    }
                    hc.setAlpha(a);

                    const double w = base_w + (peak_w - base_w) * ktime;

                    draw_poly_with_points(
                        p, l.pts_pct, hc, l.closed, Qt::SolidLine, w
                    );
                } else {
                    for (size_t i = 1; i < l.pts_pct.size(); ++i) {
                        const QPointF a_pct = l.pts_pct[i - 1];
                        const QPointF b_pct = l.pts_pct[i];

                        double k = segment_impact_k(
                            a_pct, b_pct, hit_points, falloff_pct, ktime
                        );
                        if (k <= 0.0) {
                            continue;
                        }

                        k = k * k_boost;
                        if (k > 1.0) {
                            k = 1.0;
                        }

                        QColor hc = effective_color;
                        int a = static_cast<int>(255.0 * k);
                        if (a < 0) {
                            a = 0;
                        }
                        hc.setAlpha(a);

                        const double w = base_w + (peak_w - base_w) * k;

                        std::vector<QPointF> seg { a_pct, b_pct };
                        draw_poly_with_points(
                            p, seg, hc, false, Qt::SolidLine, w
                        );
                    }

                    if (l.closed && l.pts_pct.size() >= 2) {
                        const QPointF a_pct = l.pts_pct.back();
                        const QPointF b_pct = l.pts_pct.front();

                        double k = segment_impact_k(
                            a_pct, b_pct, hit_points, falloff_pct, ktime
                        );
                        if (k > 0.0) {
                            k = k * k_boost;
                            if (k > 1.0) {
                                k = 1.0;
                            }
                            QColor hc = effective_color;
                            int a = static_cast<int>(255.0 * k);
                            if (a < 0) {
                                a = 0;
                            }
                            hc.setAlpha(a);

                            const double w = base_w + (peak_w - base_w) * k;

                            std::vector<QPointF> seg { a_pct, b_pct };
                            draw_poly_with_points(
                                p, seg, hc, false, Qt::SolidLine, w
                            );
                        }
                    }
                }
            }
        }

        draw_poly_with_points(
            p, l.pts_pct, effective_color, l.closed, Qt::SolidLine,
            line_width_visual_value(l.width_text)
        );
        if (processing_overlay_renderer::tripwire_waves_visible(
                stream_settings_value
            )) {
            draw_wave_overlay(p, l, effective_color);
        }

        if (!(active && labels_enabled)) {
            continue;
        }

        const auto& text = key;
        if (text.isEmpty()) {
            continue;
        }

        p.setPen(effective_color);
        p.drawText(label_pos_px(l), text);
    }
}

void stream_cell::draw_draft(QPainter& p) const {
    if (draft_line_points_pct.empty() || line_edit_preview_.has_value()) {
        return;
    }

    const int line_count = enabled_line_count(persistent_lines);
    const QColor effective_color = draft_color_for(
        draft_line_color_mode_id, draft_line_color, draft_line_points_pct,
        last_frame, line_count, line_count + 1
    );
    draw_poly_with_points(
        p, draft_line_points_pct, effective_color, draft_line_closed,
        Qt::DashLine, line_width_visual_value(draft_line_width_text)
    );
}

void stream_cell::draw_line_edit_preview(QPainter& p) const {
    if (!line_edit_preview_.has_value()
        || line_edit_preview_->pts_pct.empty()) {
        return;
    }

    const auto& line_value = *line_edit_preview_;
    const int line_count = enabled_line_count(persistent_lines);
    const QColor effective_color = draft_color_for(
        line_value.color_mode_id, line_value.color, line_value.pts_pct,
        last_frame, line_count, line_count + 1
    );
    if (line_edit_source_line_.has_value()
        && line_edit_source_line_->pts_pct.size() >= 2) {
        const auto& source_line = *line_edit_source_line_;
        QColor source_color = draft_color_for(
            source_line.color_mode_id, source_line.color, source_line.pts_pct,
            last_frame, line_count, line_count + 1
        );
        const double pulse
            = (std::sin(static_cast<double>(animation_clock.elapsed()) / 260.0)
               + 1.0)
            / 2.0;
        source_color.setAlpha(static_cast<int>(70.0 + 60.0 * pulse));
        draw_poly_with_points(
            p, source_line.pts_pct, source_color, source_line.closed,
            Qt::DotLine,
            std::max<qreal>(
                1.0,
                line_width_visual_value(source_line.width_text) * 0.8
                    + static_cast<qreal>(pulse)
            ),
            false
        );
    }

    QColor outline_color = contrasting_text_color(effective_color);
    outline_color.setAlpha(150);
    draw_poly_with_points(
        p, line_value.pts_pct, outline_color, line_value.closed, Qt::SolidLine,
        line_width_visual_value(line_value.width_text) + 2.0, false
    );
    draw_poly_with_points(
        p, line_value.pts_pct, effective_color, line_value.closed,
        Qt::SolidLine, line_width_visual_value(line_value.width_text) + 0.5
    );

    const auto selected_vertex = line_edit_interaction_.selected_vertex();
    if (!selected_vertex.has_value() || *selected_vertex < 0
        || *selected_vertex >= static_cast<int>(line_value.pts_pct.size())) {
        return;
    }

    const QPointF selected_px = to_px(line_value.pts_pct.at(
        static_cast<std::vector<QPointF>::size_type>(*selected_vertex)
    ));
    QPen outline_pen(contrasting_text_color(effective_color));
    outline_pen.setWidthF(2.0);
    p.setPen(outline_pen);
    p.setBrush(color_with_alpha(effective_color, 224));
    p.drawEllipse(selected_px, 6.0, 6.0);
}

void stream_cell::draw_hover_point(QPainter& p) const {
    if (!hover_point_pct.has_value()) {
        return;
    }

    auto effective_color = QColor(Qt::white);
    if (line_edit_preview_.has_value()) {
        const int line_count = enabled_line_count(persistent_lines);
        effective_color = draft_color_for(
            line_edit_preview_->color_mode_id, line_edit_preview_->color,
            line_edit_preview_->pts_pct, last_frame, line_count, line_count + 1
        );
    } else {
        const int line_count = enabled_line_count(persistent_lines);
        effective_color = draft_color_for(
            draft_line_color_mode_id, draft_line_color, draft_line_points_pct,
            last_frame, line_count, line_count + 1
        );
    }
    QPen hpen(effective_color);
    hpen.setWidthF(1.0);
    hpen.setStyle(Qt::DashLine);
    p.setPen(hpen);

    p.drawEllipse(to_px(*hover_point_pct), 4.0, 4.0);
}

void stream_cell::draw_preview_segment(QPainter& p) const {
    if (!(drawing_enabled && active && hover_point_pct.has_value()
          && !draft_line_points_pct.empty())) {
        return;
    }

    const QColor effective_color = draft_color_for(
        draft_line_color_mode_id, draft_line_color, draft_line_points_pct,
        last_frame, enabled_line_count(persistent_lines),
        enabled_line_count(persistent_lines) + 1
    );
    QPen pen(effective_color);
    pen.setWidthF(
        std::max(1.5, line_width_visual_value(draft_line_width_text) * 0.75)
    );
    pen.setStyle(Qt::DashLine);
    p.setPen(pen);

    const QPointF last_px = to_px(draft_line_points_pct.back());
    const QPointF hover_px = to_px(*hover_point_pct);

    p.drawLine(last_px, hover_px);

    if (draft_line_closed && draft_line_points_pct.size() >= 2) {
        const QPointF first_px = to_px(draft_line_points_pct.front());
        p.drawLine(hover_px, first_px);
    }
}

QPointF stream_cell::label_pos_px(const line_instance& l) const {
    if (l.pts_pct.empty()) {
        return {};
    }

    const QPointF anchor_pct = l.closed ? l.pts_pct.back() : l.pts_pct.front();
    const QPointF anchor_px = to_px(anchor_pct);
    return { anchor_px.x() + 6.0, anchor_px.y() + 14.0 };
}
