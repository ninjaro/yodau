#include "widgets/stream_cell.hpp"

#include <QBrush>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QStyle>
#include <QStyleOption>
#include <algorithm>
#include <cmath>

namespace stream_cell_support {

QPointF to_image_px(const QPointF& pos_pct, const QSize& size) {
    return { pos_pct.x() / 100.0 * static_cast<double>(size.width()),
             pos_pct.y() / 100.0 * static_cast<double>(size.height()) };
}

QColor log_badge_color(const app_log_mode mode) {
    return mode == app_log_mode::debug ? QColor(QStringLiteral("#bc6c25"))
                                       : QColor(QStringLiteral("#3a5a40"));
}

QRect hover_label_rect(
    const QRect& bounds, const QSize& label_size, const QPoint& anchor
) {
    const int horizontal_gap = 12;
    const int vertical_gap = 12;
    int left = anchor.x() + horizontal_gap;
    int top = anchor.y() - label_size.height() - vertical_gap;

    if (left + label_size.width() > bounds.right()) {
        left = anchor.x() - label_size.width() - horizontal_gap;
    }
    if (top < bounds.top()) {
        top = anchor.y() + vertical_gap;
    }

    left = std::clamp(
        left, bounds.left(),
        std::max(bounds.left(), bounds.right() - label_size.width())
    );
    top = std::clamp(
        top, bounds.top(),
        std::max(bounds.top(), bounds.bottom() - label_size.height())
    );

    return { QPoint(left, top), label_size };
}

QPolygonF
image_polygon(const std::vector<QPointF>& pts_pct, const QSize& size) {
    QPolygonF poly;
    poly.reserve(static_cast<int>(pts_pct.size()));
    for (const QPointF& pt_pct : pts_pct) {
        poly << to_image_px(pt_pct, size);
    }
    return poly;
}

qreal image_pen_width(
    const qreal display_width, const QSize& display_size,
    const QSize& image_size
) {
    if (display_size.width() <= 0 || display_size.height() <= 0) {
        return std::max<qreal>(1.0, display_width);
    }

    const qreal scale_x = static_cast<qreal>(image_size.width())
        / static_cast<qreal>(display_size.width());
    const qreal scale_y = static_cast<qreal>(image_size.height())
        / static_cast<qreal>(display_size.height());
    return std::max<qreal>(1.0, display_width * (scale_x + scale_y) * 0.5);
}

void invert_content(
    QImage& image, const QSize& display_size,
    const std::vector<QPointF>& pts_pct, const bool closed,
    const Qt::PenStyle style, const qreal display_width,
    const bool fill_closed_area
) {
    if (image.isNull() || pts_pct.size() < 2) {
        return;
    }

    const QPolygonF poly = image_polygon(pts_pct, image.size());
    if (poly.size() < 2) {
        return;
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setCompositionMode(QPainter::CompositionMode_Difference);

    QPen invert_pen(Qt::white);
    invert_pen.setWidthF(
        image_pen_width(display_width, display_size, image.size())
    );
    invert_pen.setStyle(style);
    invert_pen.setCapStyle(Qt::RoundCap);
    invert_pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(invert_pen);
    painter.setBrush(
        fill_closed_area && closed && style == Qt::SolidLine ? QBrush(Qt::white)
                                                             : Qt::NoBrush
    );

    if (closed && poly.size() >= 3) {
        painter.drawPolygon(poly);
    } else {
        painter.drawPolyline(poly);
    }
}

bool uses_content_inversion(const QString& color_mode_id) {
    return normalized_line_color_mode_id(color_mode_id)
        == QStringLiteral("negative_auto");
}

} // namespace stream_cell_support

void stream_cell::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QStyleOption opt;
    opt.initFrom(this);

    const auto now = QDateTime::currentDateTime();

    for (auto it = line_highlights.begin(); it != line_highlights.end();) {
        const int age = static_cast<int>(it.value().msecsTo(now));
        if (age >= line_highlight_ttl_ms) {
            it = line_highlights.erase(it);
        } else {
            ++it;
        }
    }

    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);

    if (!last_frame.isNull()) {
        const bool has_content_inversion
            = std::any_of(
                  persistent_lines.cbegin(), persistent_lines.cend(),
                  [](const line_instance& line_value) {
                      return line_value.enabled
                          && !line_value.template_name.trimmed().isEmpty()
                          && stream_cell_support::uses_content_inversion(
                                 line_value.color_mode_id
                          );
                  }
              )
            && std::any_of(
                  persistent_lines.cbegin(), persistent_lines.cend(),
                  [this](const line_instance& line_value) {
                      const QString key = line_value.template_name.trimmed();
                      return line_value.enabled && !key.isEmpty()
                          && line_highlights.contains(key)
                          && stream_cell_support::uses_content_inversion(
                                 line_value.color_mode_id
                          );
                  }
            );

        if (has_content_inversion) {
            QImage composited_frame = last_frame;

            for (const auto& line_value : persistent_lines) {
                const QString key = line_value.template_name.trimmed();
                if (!line_value.enabled || key.isEmpty()
                    || !line_highlights.contains(key)
                    || !stream_cell_support::uses_content_inversion(
                        line_value.color_mode_id
                    )) {
                    continue;
                }

                stream_cell_support::invert_content(
                    composited_frame, display_frame_rect().size().toSize(),
                    line_value.pts_pct, line_value.closed, Qt::SolidLine,
                    line_width_visual_value(line_value.width_text), true
                );
            }

            draw_frame(p, composited_frame);
        } else {
            draw_frame(p, last_frame);
        }
    } else {
        const QString txt = last_error.isEmpty() ? "no signal" : last_error;
        const QRect r = rect().adjusted(6, 6, -6, -6);
        p.setPen(palette().color(QPalette::Text));
        p.drawText(r, Qt::AlignCenter, txt);
    }

    p.drawRect(rect().adjusted(0, 0, -1, -1));
    p.setRenderHint(QPainter::Antialiasing, true);

    prune_line_waves(animation_clock.elapsed());
    update_animation_timer();

    draw_persistent(p);
    draw_processing_overlays(p);
    draw_events(p);
    draw_draft(p);
    draw_line_edit_preview(p);
    draw_preview_segment(p);
    draw_stream_name(p);
    draw_runtime_metrics(p);
    draw_hover_point(p);
    draw_hover_coords(p);
}

void stream_cell::draw_frame(QPainter& painter, const QImage& frame) const {
    if (frame.isNull()) {
        return;
    }

    const QRectF target = display_frame_rect();
    painter.save();
    painter.setClipRect(target);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    switch (quarter_turns_) {
    case 1:
        painter.translate(target.right(), target.top());
        painter.rotate(90.0);
        painter.drawImage(
            QRectF(0.0, 0.0, target.height(), target.width()), frame
        );
        break;
    case 2:
        painter.translate(target.right(), target.bottom());
        painter.rotate(180.0);
        painter.drawImage(
            QRectF(0.0, 0.0, target.width(), target.height()), frame
        );
        break;
    case 3:
        painter.translate(target.left(), target.bottom());
        painter.rotate(270.0);
        painter.drawImage(
            QRectF(0.0, 0.0, target.height(), target.width()), frame
        );
        break;
    default:
        painter.drawImage(target, frame);
        break;
    }

    painter.restore();
}

void stream_cell::draw_hover_coords(QPainter& p) const {
    if (!(hover_point_pct.has_value()
          && (drawing_enabled || line_edit_preview_.has_value()) && active
          && stream_settings_value.standard_labels_enabled)) {
        return;
    }

    const auto& hp = *hover_point_pct;
    const int pixel_width = !last_frame.isNull() ? last_frame.width() : width();
    const int pixel_height
        = !last_frame.isNull() ? last_frame.height() : height();
    const int x_px = std::clamp(
        static_cast<int>(
            std::lround(hp.x() / 100.0 * std::max(0, pixel_width - 1))
        ),
        0, std::max(0, pixel_width - 1)
    );
    const int y_px = std::clamp(
        static_cast<int>(
            std::lround(hp.y() / 100.0 * std::max(0, pixel_height - 1))
        ),
        0, std::max(0, pixel_height - 1)
    );

    const QString txt = QStringLiteral(
                            "x %1%% | %2 px\n"
                            "y %3%% | %4 px"
    )
                            .arg(hp.x(), 0, 'f', 1)
                            .arg(x_px)
                            .arg(hp.y(), 0, 'f', 1)
                            .arg(y_px);

    auto line_color = QColor(Qt::white);
    if (line_edit_preview_.has_value()) {
        line_color = draft_color_for(
            line_edit_preview_->color_mode_id, line_edit_preview_->color,
            line_edit_preview_->pts_pct, last_frame,
            enabled_line_count(persistent_lines),
            enabled_line_count(persistent_lines) + 1
        );
    } else {
        std::vector<QPointF> preview_points = draft_line_points_pct;
        preview_points.push_back(hp);
        line_color = draft_color_for(
            draft_line_color_mode_id, draft_line_color, preview_points,
            last_frame, enabled_line_count(persistent_lines),
            enabled_line_count(persistent_lines) + 1
        );
    }
    QColor bubble_fill = line_color;
    bubble_fill.setAlpha(220);
    const QColor bubble_border = line_color.darker(145);
    const QColor text_color = contrasting_text_color(bubble_fill);

    QFont hover_font = p.font();
    const double hover_point_size
        = hover_font.pointSizeF() > 0.0 ? hover_font.pointSizeF() : 9.0;
    hover_font.setPointSizeF(std::max(7.5, hover_point_size - 0.5));
    p.setFont(hover_font);

    const QRect bounds = rect().adjusted(6, 6, -6, -6);
    const QFontMetrics metrics(hover_font);
    QRect text_rect = metrics.boundingRect(
        QRect(0, 0, std::max(140, bounds.width() - 24), bounds.height()),
        Qt::TextWordWrap, txt
    );
    text_rect.adjust(-8, -6, 8, 6);
    const QRect bubble_rect = stream_cell_support::hover_label_rect(
        bounds, text_rect.size(), to_px(hp).toPoint()
    );

    p.setPen(QPen(bubble_border, 1.0));
    p.setBrush(bubble_fill);
    p.drawRoundedRect(bubble_rect, 8.0, 8.0);

    p.setPen(text_color);
    p.drawText(
        bubble_rect.adjusted(8, 6, -8, -6),
        Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, txt
    );
}

void stream_cell::draw_stream_name(QPainter& p) const {
    if (name.isEmpty() || !stream_settings_value.standard_labels_enabled) {
        return;
    }

    QRect r = rect().adjusted(6, 6, -6, -6);
    p.setPen(palette().color(QPalette::Text));
    p.drawText(r, Qt::AlignLeft | Qt::AlignTop, name);

    const QString badge_text = algorithm_badge_text(
        stream_settings_value.algorithm_id,
        stream_settings_value.algorithm_preset,
        movement_display_enabled(stream_settings_value.movement_display_mode)
    );
    const QColor badge_color
        = algorithm_badge_color(stream_settings_value.algorithm_id);
    const QString log_badge_text
        = QStringLiteral("log %1").arg(app_log_mode_name(log_mode_value));
    const QColor log_badge_fill
        = stream_cell_support::log_badge_color(log_mode_value);

    QFont badge_font = p.font();
    const double badge_point_size
        = badge_font.pointSizeF() > 0.0 ? badge_font.pointSizeF() : 9.0;
    badge_font.setPointSizeF(std::max(8.0, badge_point_size - 0.5));
    p.setFont(badge_font);

    const QFontMetrics badge_metrics(badge_font);
    QRect badge_rect = badge_metrics.boundingRect(badge_text);
    badge_rect.adjust(-8, -4, 8, 4);
    badge_rect.moveBottomRight(QPoint(r.right(), r.bottom()));

    QRect log_badge_rect = badge_metrics.boundingRect(log_badge_text);
    log_badge_rect.adjust(-8, -4, 8, 4);
    log_badge_rect.moveBottomRight(QPoint(r.right(), badge_rect.top() - 6));

    QColor badge_fill = badge_color;
    badge_fill.setAlpha(active ? 208 : 176);
    p.setPen(Qt::NoPen);
    p.setBrush(color_with_alpha(log_badge_fill, active ? 188 : 160));
    p.drawRoundedRect(log_badge_rect, 8.0, 8.0);
    p.setPen(Qt::white);
    p.drawText(log_badge_rect, Qt::AlignCenter, log_badge_text);

    p.setPen(Qt::NoPen);
    p.setBrush(badge_fill);
    p.drawRoundedRect(badge_rect, 8.0, 8.0);

    p.setPen(Qt::white);
    p.drawText(badge_rect, Qt::AlignCenter, badge_text);

    if (!active) {
        return;
    }

    QFont summary_font = p.font();
    const double summary_point_size
        = summary_font.pointSizeF() > 0.0 ? summary_font.pointSizeF() : 9.0;
    summary_font.setPointSizeF(std::max(8.0, summary_point_size - 1.0));
    p.setFont(summary_font);
    p.setPen(palette().color(QPalette::Text));

    const QString summary = algorithm_summary_text(
        stream_settings_value.algorithm_id,
        stream_settings_value.algorithm_preset,
        stream_settings_value.movement_display_mode
    );
    const QFontMetrics summary_metrics(summary_font);
    const QString elided_summary = summary_metrics.elidedText(
        summary, Qt::ElideRight, std::max(80, r.width() - 12)
    );

    QRect summary_rect = r;
    summary_rect.adjust(0, 18, 0, 0);
    p.drawText(summary_rect, Qt::AlignLeft | Qt::AlignTop, elided_summary);
}

void stream_cell::draw_runtime_metrics(QPainter& p) const {
    if (!stream_settings_value.standard_labels_enabled) {
        return;
    }

    const QString metrics_text
        = stream_runtime_metrics_text(runtime_metrics_value);
    if (metrics_text.trimmed().isEmpty()) {
        return;
    }

    QFont metrics_font = p.font();
    const double metrics_point_size
        = metrics_font.pointSizeF() > 0.0 ? metrics_font.pointSizeF() : 9.0;
    metrics_font.setPointSizeF(std::max(7.5, metrics_point_size - 1.0));
    p.setFont(metrics_font);

    const QRect bounds = rect().adjusted(6, 6, -6, -6);
    const QFontMetrics metrics(metrics_font);
    const QString rendered_text = (!active && bounds.width() < 220)
        ? QStringLiteral("v %1 | b %2")
              .arg(
                  runtime_metrics_value.input_fps > 0.0
                      ? QString::number(runtime_metrics_value.input_fps, 'f', 1)
                      : QStringLiteral("--")
              )
              .arg(
                  runtime_metrics_value.core_fps > 0.0
                      ? QString::number(runtime_metrics_value.core_fps, 'f', 1)
                      : QStringLiteral("--")
              )
        : metrics_text;

    QRect metrics_rect = metrics.boundingRect(
        QRect(0, 0, std::max(120, bounds.width() - 80), bounds.height()),
        Qt::TextWordWrap, rendered_text
    );
    metrics_rect.adjust(-8, -4, 8, 4);
    metrics_rect.moveBottomLeft(QPoint(bounds.left(), bounds.bottom()));

    QColor metrics_fill(Qt::black);
    metrics_fill.setAlpha(active ? 164 : 136);
    p.setPen(Qt::NoPen);
    p.setBrush(metrics_fill);
    p.drawRoundedRect(metrics_rect, 8.0, 8.0);

    p.setPen(Qt::white);
    p.drawText(metrics_rect, Qt::AlignLeft | Qt::AlignVCenter, rendered_text);
}
