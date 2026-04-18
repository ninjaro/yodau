#include "widgets/processing_overlay_renderer.hpp"

#include <QColor>
#include <QHash>
#include <QLineF>
#include <QPainter>
#include <QPen>
#include <QPolygonF>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

QString automatic_movement_mode(const stream_settings& settings_value) {
    const QString algorithm_id = normalized_app_algorithm_id(
        settings_value.algorithm_id
    );
    const QString preset_id = normalized_algorithm_preset_id(
        algorithm_id, settings_value.algorithm_preset
    );

    if (algorithm_id == QStringLiteral("spot_grid")) {
        return QStringLiteral("bubbles");
    }
    if (algorithm_id == QStringLiteral("contour_mask")) {
        return QStringLiteral("contours");
    }
    if (algorithm_id == QStringLiteral("centroid_track")) {
        return preset_id == QStringLiteral("fast_match")
            ? QStringLiteral("vectors")
            : QStringLiteral("tracks");
    }
    if (algorithm_id == QStringLiteral("hybrid_auto")) {
        return QStringLiteral("auto");
    }
    return QStringLiteral("bubbles");
}

bool backend_overlays_visible(const QString& mode_id) {
    const QString mode = normalized_movement_display_mode_id(mode_id);
    return mode != QStringLiteral("off")
        && mode != QStringLiteral("tripwire_waves");
}

bool should_draw_overlay(
    const processing_overlay_instance& overlay, const QString& mode_id
) {
    const QString mode = normalized_movement_display_mode_id(mode_id);
    if (mode == QStringLiteral("auto")) {
        return true;
    }
    if (mode == QStringLiteral("bubbles")) {
        return overlay.kind == processing_overlay_kind::point;
    }
    if (mode == QStringLiteral("contours")) {
        return overlay.kind == processing_overlay_kind::polygon;
    }
    if (mode == QStringLiteral("vectors")
        || mode == QStringLiteral("tracks")) {
        return overlay.kind == processing_overlay_kind::polyline
            || overlay.kind == processing_overlay_kind::point;
    }
    return false;
}

QColor color_with_alpha(QColor color, const int alpha) {
    color.setAlpha(std::clamp(alpha, 0, 255));
    return color;
}

QColor overlay_color_for_label(const QString& label) {
    const auto seed = qHash(label.isEmpty() ? QStringLiteral("overlay") : label);
    return QColor::fromHsv(static_cast<int>(seed % 360U), 165, 235);
}

bool is_flow_overlay(const QString& label) {
    return label.startsWith(QStringLiteral("flow_"));
}

bool is_average_flow_overlay(const QString& label) {
    return label == QStringLiteral("flow_average");
}

bool is_track_overlay(const QString& label) {
    return label.startsWith(QStringLiteral("track"));
}

QPointF to_px(const QRectF& bounds, const QPointF& point_pct) {
    return {
        bounds.left() + point_pct.x() / 100.0 * bounds.width(),
        bounds.top() + point_pct.y() / 100.0 * bounds.height(),
    };
}

void draw_arrow_head(
    QPainter& painter, const QPointF& from, const QPointF& to,
    const QColor& color
) {
    const QLineF line(from, to);
    if (line.length() < 2.0) {
        return;
    }

    const double angle = std::atan2(to.y() - from.y(), to.x() - from.x());
    constexpr double size = 9.0;
    const QPointF left(
        to.x() - std::cos(angle - std::numbers::pi / 6.0) * size,
        to.y() - std::sin(angle - std::numbers::pi / 6.0) * size
    );
    const QPointF right(
        to.x() - std::cos(angle + std::numbers::pi / 6.0) * size,
        to.y() - std::sin(angle + std::numbers::pi / 6.0) * size
    );

    QPolygonF arrow;
    arrow << to << left << right;
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawPolygon(arrow);
}

} // namespace

namespace processing_overlay_renderer {

QString selected_movement_mode(const stream_settings& settings_value) {
    return normalized_movement_display_mode_id(settings_value.movement_display_mode);
}

QString resolved_backend_overlay_mode(const stream_settings& settings_value) {
    const QString mode = selected_movement_mode(settings_value);
    if (mode == QStringLiteral("auto")) {
        return automatic_movement_mode(settings_value);
    }
    return mode;
}

bool event_bubbles_visible(const stream_settings& settings_value) {
    const QString mode = resolved_backend_overlay_mode(settings_value);
    return mode != QStringLiteral("off")
        && mode != QStringLiteral("tripwire_waves");
}

bool tripwire_waves_visible(const stream_settings& settings_value) {
    const QString mode = selected_movement_mode(settings_value);
    return mode == QStringLiteral("auto")
        || mode == QStringLiteral("tripwire_waves");
}

void draw(
    QPainter& painter, const QRectF& bounds,
    const stream_settings& settings_value,
    const std::vector<processing_overlay_instance>& overlays
) {
    const QString mode = resolved_backend_overlay_mode(settings_value);
    if (!backend_overlays_visible(mode) || overlays.empty()) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (const auto& overlay : overlays) {
        if (!should_draw_overlay(overlay, mode)) {
            continue;
        }

        QColor color = overlay_color_for_label(overlay.label);
        color.setAlpha(185);

        if (overlay.kind == processing_overlay_kind::point) {
            if (!overlay.anchor_pct.has_value()) {
                continue;
            }
            const QPointF center = to_px(bounds, *overlay.anchor_pct);
            const bool track_point = is_track_overlay(overlay.label);
            painter.setPen(
                QPen(color.lighter(135), track_point ? 1.9 : 1.6)
            );
            painter.setBrush(
                color_with_alpha(color, track_point ? 112 : 82)
            );
            painter.drawEllipse(
                center, track_point ? 7.0 : 9.0, track_point ? 7.0 : 9.0
            );
            if (track_point) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(color_with_alpha(color.lighter(130), 180));
                painter.drawEllipse(center, 2.5, 2.5);
            }
            continue;
        }

        if (overlay.kind == processing_overlay_kind::label) {
            if (!overlay.anchor_pct.has_value()) {
                continue;
            }
            const QPointF anchor = to_px(bounds, *overlay.anchor_pct);
            painter.setPen(color.lighter(145));
            painter.drawText(anchor + QPointF(8.0, -8.0), overlay.label);
            continue;
        }

        if (overlay.points_pct.size() < 2) {
            continue;
        }

        QPolygonF path;
        path.reserve(static_cast<int>(overlay.points_pct.size()));
        for (const auto& point_pct : overlay.points_pct) {
            path << to_px(bounds, point_pct);
        }

        const bool flow_overlay = is_flow_overlay(overlay.label);
        const bool average_flow = is_average_flow_overlay(overlay.label);
        const bool track_overlay = is_track_overlay(overlay.label);
        QPen pen(color);
        pen.setWidthF(
            average_flow ? 2.8
            : (flow_overlay ? 1.7
                            : (mode == QStringLiteral("vectors") ? 2.1 : 1.8))
        );
        pen.setStyle(
            flow_overlay
                ? Qt::DashLine
                : (mode == QStringLiteral("tracks") || track_overlay
                       ? Qt::SolidLine
                       : Qt::DashLine)
        );
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);

        if (overlay.kind == processing_overlay_kind::polygon) {
            painter.setBrush(color_with_alpha(color, 34));
            painter.drawPolygon(path);
            continue;
        }

        painter.setBrush(Qt::NoBrush);
        painter.drawPolyline(path);

        if ((flow_overlay || mode == QStringLiteral("vectors"))
            && path.size() >= 2) {
            draw_arrow_head(
                painter, path.at(path.size() - 2), path.at(path.size() - 1),
                color
            );
        } else if (mode == QStringLiteral("tracks") || track_overlay) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(color_with_alpha(color, 135));
            for (const QPointF& point_value : path) {
                painter.drawEllipse(point_value, 3.5, 3.5);
            }
        }
    }

    painter.restore();
}

} // namespace processing_overlay_renderer
