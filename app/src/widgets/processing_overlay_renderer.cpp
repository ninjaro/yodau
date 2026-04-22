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
#include <optional>

namespace {

enum class overlay_layer { bubbles, contours, vectors, tracks, labels, unknown };

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

QColor color_with_alpha(QColor color, const int alpha) {
    color.setAlpha(std::clamp(alpha, 0, 255));
    return color;
}

QColor overlay_color_for_label(const QString& label) {
    const auto seed = qHash(label.isEmpty() ? QStringLiteral("overlay") : label);
    return QColor::fromHsv(static_cast<int>(seed % 360U), 165, 235);
}

QString normalized_overlay_label(const QString& label) {
    return label.trimmed().toLower();
}

bool is_flow_overlay(const QString& label) {
    return normalized_overlay_label(label).startsWith(QStringLiteral("flow_"));
}

bool is_average_flow_overlay(const QString& label) {
    return normalized_overlay_label(label) == QStringLiteral("flow_average");
}

bool is_track_overlay(const QString& label) {
    return normalized_overlay_label(label).startsWith(QStringLiteral("track"));
}

bool is_contour_overlay(const QString& label) {
    const QString normalized = normalized_overlay_label(label);
    return normalized.startsWith(QStringLiteral("contour"))
        || normalized == QStringLiteral("mask")
        || normalized.endsWith(QStringLiteral("_mask"));
}

overlay_layer layer_for_overlay(const processing_overlay_instance& overlay) {
    if (is_track_overlay(overlay.label)) {
        return overlay_layer::tracks;
    }
    if (is_flow_overlay(overlay.label)) {
        return overlay_layer::vectors;
    }
    if (is_contour_overlay(overlay.label)) {
        return overlay_layer::contours;
    }

    switch (overlay.kind) {
    case processing_overlay_kind::point:
        return overlay_layer::bubbles;
    case processing_overlay_kind::polyline:
        return overlay_layer::vectors;
    case processing_overlay_kind::polygon:
        return overlay_layer::contours;
    case processing_overlay_kind::label:
        return overlay_layer::labels;
    }

    return overlay_layer::unknown;
}

std::optional<QString> movement_mode_from_overlays(
    const std::vector<processing_overlay_instance>& overlays
) {
    bool has_bubbles = false;
    bool has_contours = false;
    bool has_vectors = false;
    bool has_tracks = false;

    for (const auto& overlay : overlays) {
        switch (layer_for_overlay(overlay)) {
        case overlay_layer::bubbles:
            has_bubbles = true;
            break;
        case overlay_layer::contours:
            has_contours = true;
            break;
        case overlay_layer::vectors:
            has_vectors = true;
            break;
        case overlay_layer::tracks:
            has_tracks = true;
            break;
        case overlay_layer::labels:
        case overlay_layer::unknown:
            break;
        }
    }

    if (has_tracks) {
        return QStringLiteral("tracks");
    }
    if (has_vectors) {
        return QStringLiteral("vectors");
    }
    if (has_contours) {
        return QStringLiteral("contours");
    }
    if (has_bubbles) {
        return QStringLiteral("bubbles");
    }
    return std::nullopt;
}

bool should_draw_overlay(
    const processing_overlay_instance& overlay, const QString& mode_id
) {
    const QString mode = normalized_movement_display_mode_id(mode_id);
    if (mode == QStringLiteral("auto")) {
        return true;
    }

    const overlay_layer layer = layer_for_overlay(overlay);
    if (mode == QStringLiteral("bubbles")) {
        return layer == overlay_layer::bubbles;
    }
    if (mode == QStringLiteral("contours")) {
        return layer == overlay_layer::contours;
    }
    if (mode == QStringLiteral("vectors")) {
        return layer == overlay_layer::vectors;
    }
    if (mode == QStringLiteral("tracks")) {
        return layer == overlay_layer::tracks;
    }
    return false;
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

double event_region_scale(const stream_settings& settings_value) {
    const QString algorithm_id = normalized_app_algorithm_id(
        settings_value.algorithm_id
    );
    const QString preset_id = normalized_algorithm_preset_id(
        algorithm_id, settings_value.algorithm_preset
    );

    if (algorithm_id == QStringLiteral("spot_grid")) {
        if (preset_id == QStringLiteral("coarse")) {
            return 0.12;
        }
        if (preset_id == QStringLiteral("dense")) {
            return 0.2;
        }
        return 0.16;
    }

    if (algorithm_id == QStringLiteral("hybrid_auto")) {
        if (preset_id == QStringLiteral("load_guard")) {
            return 0.12;
        }
        if (preset_id == QStringLiteral("tripwire_bias")) {
            return 0.19;
        }
        return 0.16;
    }

    if (algorithm_id == QStringLiteral("contour_mask")) {
        if (preset_id == QStringLiteral("outline")) {
            return 0.14;
        }
        if (preset_id == QStringLiteral("mask_heavy")) {
            return 0.22;
        }
        return 0.18;
    }

    if (algorithm_id == QStringLiteral("centroid_track")) {
        if (preset_id == QStringLiteral("fast_match")) {
            return 0.13;
        }
        if (preset_id == QStringLiteral("persistent")) {
            return 0.21;
        }
        return 0.17;
    }

    if (preset_id == QStringLiteral("simple")) {
        return 0.11;
    }
    if (preset_id == QStringLiteral("debug")) {
        return 0.2;
    }
    return 0.15;
}

int spot_grid_dimension(const stream_settings& settings_value) {
    const QString preset_id = normalized_algorithm_preset_id(
        settings_value.algorithm_id, settings_value.algorithm_preset
    );

    if (preset_id == QStringLiteral("coarse")) {
        return 2;
    }
    if (preset_id == QStringLiteral("dense")) {
        return 4;
    }
    return 3;
}

QRectF event_region_rect(
    const QRect& rect_value, const QPointF& center,
    const stream_settings& settings_value
) {
    const double scale = event_region_scale(settings_value);
    const double region_width = rect_value.width() * scale;
    const double region_height = rect_value.height() * scale;

    QRectF region(
        center.x() - region_width * 0.5, center.y() - region_height * 0.5,
        region_width, region_height
    );

    const QRectF bounds = rect_value.adjusted(8, 8, -8, -8);
    if (region.left() < bounds.left()) {
        region.moveLeft(bounds.left());
    }
    if (region.top() < bounds.top()) {
        region.moveTop(bounds.top());
    }
    if (region.right() > bounds.right()) {
        region.moveRight(bounds.right());
    }
    if (region.bottom() > bounds.bottom()) {
        region.moveBottom(bounds.bottom());
    }

    return region;
}

void draw_baseline_event_bubble(
    QPainter& painter, const QPointF& center, const double radius,
    const QColor& color, const stream_settings& settings_value,
    const double life_k
) {
    const QString preset_id = normalized_algorithm_preset_id(
        settings_value.algorithm_id, settings_value.algorithm_preset
    );
    const int ring_count = preset_id == QStringLiteral("debug") ? 3 : 2;
    const double spread = preset_id == QStringLiteral("simple") ? 2.2 : 3.0;

    for (int i = 0; i < ring_count; i += 1) {
        const double factor
            = 1.0 + spread * (static_cast<double>(i) / ring_count);
        QPen pen(color_with_alpha(color, static_cast<int>(180.0 * life_k)));
        pen.setWidthF(1.5 + i * 0.7);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(center, radius * factor, radius * factor);
    }

    if (preset_id == QStringLiteral("debug")) {
        QPen cross_pen(color_with_alpha(color, static_cast<int>(160.0 * life_k)));
        cross_pen.setWidthF(1.0);
        cross_pen.setStyle(Qt::DashLine);
        painter.setPen(cross_pen);
        painter.drawLine(
            QPointF(center.x() - radius * 4.0, center.y()),
            QPointF(center.x() + radius * 4.0, center.y())
        );
        painter.drawLine(
            QPointF(center.x(), center.y() - radius * 4.0),
            QPointF(center.x(), center.y() + radius * 4.0)
        );
    }
}

void draw_spot_grid_event_bubble(
    QPainter& painter, const QRectF& region, const QPointF& center,
    const double radius, const QColor& color,
    const stream_settings& settings_value, const double life_k
) {
    const int dimension = spot_grid_dimension(settings_value);
    const double cell_width = region.width() / dimension;
    const double cell_height = region.height() / dimension;

    QPen border_pen(color_with_alpha(color, static_cast<int>(150.0 * life_k)));
    border_pen.setWidthF(1.2);
    painter.setPen(border_pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(region, 6.0, 6.0);

    for (int row = 1; row < dimension; row += 1) {
        painter.drawLine(
            QPointF(region.left(), region.top() + row * cell_height),
            QPointF(region.right(), region.top() + row * cell_height)
        );
    }
    for (int col = 1; col < dimension; col += 1) {
        painter.drawLine(
            QPointF(region.left() + col * cell_width, region.top()),
            QPointF(region.left() + col * cell_width, region.bottom())
        );
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(color_with_alpha(color, static_cast<int>(96.0 * life_k)));

    for (int row = 0; row < dimension; row += 1) {
        for (int col = 0; col < dimension; col += 1) {
            const QPointF spot_center(
                region.left() + (col + 0.5) * cell_width,
                region.top() + (row + 0.5) * cell_height
            );
            painter.drawEllipse(spot_center, radius * 0.45, radius * 0.45);
        }
    }

    painter.setBrush(color_with_alpha(color, static_cast<int>(180.0 * life_k)));
    painter.drawEllipse(center, radius * 0.9, radius * 0.9);
}

void draw_contour_event_bubble(
    QPainter& painter, const QRectF& region, const QPointF& center,
    const double radius, const QColor& color,
    const stream_settings& settings_value, const double life_k
) {
    const QString preset_id = normalized_algorithm_preset_id(
        settings_value.algorithm_id, settings_value.algorithm_preset
    );

    if (preset_id == QStringLiteral("mask_heavy")) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color_with_alpha(color, static_cast<int>(42.0 * life_k)));
        painter.drawRoundedRect(region, 10.0, 10.0);
    }

    QPen mask_pen(color_with_alpha(color, static_cast<int>(170.0 * life_k)));
    mask_pen.setWidthF(preset_id == QStringLiteral("outline") ? 1.6 : 2.2);
    mask_pen.setStyle(
        preset_id == QStringLiteral("outline") ? Qt::DashLine : Qt::SolidLine
    );
    painter.setPen(mask_pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(region, 10.0, 10.0);

    QPolygonF contour;
    contour << QPointF(center.x(), region.top())
            << QPointF(region.right(), center.y())
            << QPointF(center.x(), region.bottom())
            << QPointF(region.left(), center.y());
    painter.drawPolygon(contour);

    if (preset_id != QStringLiteral("outline")) {
        painter.drawEllipse(center, region.width() * 0.28, region.height() * 0.28);
    }

    painter.setPen(QPen(color_with_alpha(color, static_cast<int>(190.0 * life_k)), 1.2));
    painter.drawLine(
        QPointF(center.x() - radius * 1.8, center.y()),
        QPointF(center.x() + radius * 1.8, center.y())
    );
}

void draw_hybrid_event_bubble(
    QPainter& painter, const QRectF& region, const QPointF& center,
    const double radius, const QColor& color,
    const stream_settings& settings_value, const double life_k
) {
    const QString preset_id = normalized_algorithm_preset_id(
        settings_value.algorithm_id, settings_value.algorithm_preset
    );
    const int dimension = preset_id == QStringLiteral("load_guard")
        ? 2
        : (preset_id == QStringLiteral("tripwire_bias") ? 4 : 3);

    QPen frame_pen(color_with_alpha(color, static_cast<int>(155.0 * life_k)));
    frame_pen.setWidthF(preset_id == QStringLiteral("tripwire_bias") ? 2.0 : 1.4);
    frame_pen.setStyle(
        preset_id == QStringLiteral("load_guard") ? Qt::DotLine : Qt::DashLine
    );
    painter.setPen(frame_pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(region, 8.0, 8.0);

    const double cell_width = region.width() / dimension;
    const double cell_height = region.height() / dimension;
    QPen grid_pen(color_with_alpha(color, static_cast<int>(82.0 * life_k)));
    grid_pen.setWidthF(1.0);
    painter.setPen(grid_pen);
    for (int row = 1; row < dimension; row += 1) {
        painter.drawLine(
            QPointF(region.left(), region.top() + row * cell_height),
            QPointF(region.right(), region.top() + row * cell_height)
        );
    }
    for (int col = 1; col < dimension; col += 1) {
        painter.drawLine(
            QPointF(region.left() + col * cell_width, region.top()),
            QPointF(region.left() + col * cell_width, region.bottom())
        );
    }

    QPolygonF contour;
    contour << QPointF(center.x(), region.top())
            << QPointF(region.right(), center.y())
            << QPointF(center.x(), region.bottom())
            << QPointF(region.left(), center.y());
    QPen contour_pen(color_with_alpha(color, static_cast<int>(180.0 * life_k)));
    contour_pen.setWidthF(preset_id == QStringLiteral("tripwire_bias") ? 1.9 : 1.4);
    painter.setPen(contour_pen);
    painter.drawPolygon(contour);

    painter.setPen(Qt::NoPen);
    painter.setBrush(color_with_alpha(color, static_cast<int>(110.0 * life_k)));
    painter.drawEllipse(center, radius * 0.85, radius * 0.85);

    if (preset_id != QStringLiteral("load_guard")) {
        draw_baseline_event_bubble(
            painter, center, radius * 0.7, color, settings_value, life_k * 0.85
        );
    }
}

void draw_centroid_event_bubble(
    QPainter& painter, const QRectF& region, const QPointF& center,
    const double radius, const QColor& color,
    const stream_settings& settings_value, const double life_k
) {
    const QString preset_id = normalized_algorithm_preset_id(
        settings_value.algorithm_id, settings_value.algorithm_preset
    );
    const int segment_count = preset_id == QStringLiteral("fast_match")
        ? 3
        : (preset_id == QStringLiteral("persistent") ? 6 : 4);
    const double span_x = region.width()
        * (preset_id == QStringLiteral("persistent") ? 0.52 : 0.38);
    const double wave_y = region.height()
        * (preset_id == QStringLiteral("fast_match") ? 0.08 : 0.14);

    QPolygonF track_path;
    track_path.reserve(segment_count + 1);
    for (int index = 0; index <= segment_count; index += 1) {
        const double k
            = static_cast<double>(index) / static_cast<double>(segment_count);
        const double x = center.x() - span_x * (1.0 - k);
        const double y = center.y()
            + ((index % 2 == 0) ? -wave_y : wave_y) * (1.0 - k * 0.55);
        track_path << QPointF(x, y);
    }

    QPen tail_pen(color_with_alpha(color, static_cast<int>(165.0 * life_k)));
    tail_pen.setWidthF(preset_id == QStringLiteral("persistent") ? 2.4 : 1.8);
    tail_pen.setStyle(
        preset_id == QStringLiteral("fast_match") ? Qt::DashLine : Qt::SolidLine
    );
    painter.setPen(tail_pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPolyline(track_path);

    if (preset_id != QStringLiteral("fast_match")) {
        painter.setPen(Qt::NoPen);
        for (int index = 0; index < track_path.size() - 1; index += 1) {
            const double k = 1.0
                - static_cast<double>(index + 1)
                    / static_cast<double>(track_path.size());
            painter.setBrush(
                color_with_alpha(color, static_cast<int>((60.0 + k * 70.0) * life_k))
            );
            painter.drawEllipse(track_path.at(index), radius * 0.42, radius * 0.42);
        }
    }

    QPen head_pen(color_with_alpha(color, static_cast<int>(185.0 * life_k)));
    head_pen.setWidthF(1.3);
    painter.setPen(head_pen);
    painter.setBrush(color_with_alpha(color, static_cast<int>(145.0 * life_k)));
    painter.drawEllipse(center, radius * 1.1, radius * 1.1);
    painter.drawLine(
        QPointF(center.x() - radius * 1.6, center.y()),
        QPointF(center.x() + radius * 1.6, center.y())
    );
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

QString resolved_draw_overlay_mode(
    const stream_settings& settings_value,
    const std::vector<processing_overlay_instance>& overlays
) {
    const QString mode = selected_movement_mode(settings_value);
    if (mode != QStringLiteral("auto")) {
        return mode;
    }

    if (const auto overlay_mode = movement_mode_from_overlays(overlays);
        overlay_mode.has_value()) {
        return *overlay_mode;
    }

    return automatic_movement_mode(settings_value);
}

bool event_bubbles_visible(
    const stream_settings& settings_value,
    const std::vector<processing_overlay_instance>& overlays
) {
    return resolved_draw_overlay_mode(settings_value, overlays)
        == QStringLiteral("bubbles");
}

bool tripwire_waves_visible(const stream_settings& settings_value) {
    const QString mode = selected_movement_mode(settings_value);
    return mode == QStringLiteral("tripwire_waves");
}

void draw(
    QPainter& painter, const QRectF& bounds,
    const stream_settings& settings_value,
    const std::vector<processing_overlay_instance>& overlays
) {
    const QString mode = resolved_draw_overlay_mode(settings_value, overlays);
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

void draw_event_bubble(
    QPainter& painter, const QRect& bounds, const QPointF& center,
    const double radius, const QColor& color,
    const stream_settings& settings_value, const double life_k
) {
    const QString algorithm_id = normalized_app_algorithm_id(
        settings_value.algorithm_id
    );
    const QRectF region = event_region_rect(bounds, center, settings_value);

    if (algorithm_id == QStringLiteral("spot_grid")) {
        draw_spot_grid_event_bubble(
            painter, region, center, radius, color, settings_value, life_k
        );
        return;
    }

    if (algorithm_id == QStringLiteral("hybrid_auto")) {
        draw_hybrid_event_bubble(
            painter, region, center, radius, color, settings_value, life_k
        );
        return;
    }

    if (algorithm_id == QStringLiteral("contour_mask")) {
        draw_contour_event_bubble(
            painter, region, center, radius, color, settings_value, life_k
        );
        return;
    }

    if (algorithm_id == QStringLiteral("centroid_track")) {
        draw_centroid_event_bubble(
            painter, region, center, radius, color, settings_value, life_k
        );
        return;
    }

    draw_baseline_event_bubble(
        painter, center, radius, color, settings_value, life_k
    );
}

} // namespace processing_overlay_renderer
