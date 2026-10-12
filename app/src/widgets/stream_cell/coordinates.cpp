#include "widgets/stream_cell.hpp"

#include <algorithm>
#include <cmath>

QPointF stream_cell::clamp_pct(QPointF point_pct) {
    point_pct.setX(std::clamp(point_pct.x(), 0.0, 100.0));
    point_pct.setY(std::clamp(point_pct.y(), 0.0, 100.0));
    return point_pct;
}

QRectF stream_cell::display_frame_rect() const noexcept {
    const QRectF bounds(0.0, 0.0, width(), height());
    if (!active || !keep_frame_aspect_ratio_ || bounds.isEmpty()) {
        return bounds;
    }

    const double aspect = effective_display_aspect();
    if (!(aspect > 0.0) || !std::isfinite(aspect)) {
        return bounds;
    }

    const double available_aspect = bounds.width() / bounds.height();
    if (available_aspect > aspect) {
        const double frame_width = bounds.height() * aspect;
        return {
            (bounds.width() - frame_width) * 0.5,
            0.0,
            frame_width,
            bounds.height(),
        };
    }

    const double frame_height = bounds.width() / aspect;
    return {
        0.0,
        (bounds.height() - frame_height) * 0.5,
        bounds.width(),
        frame_height,
    };
}

QPointF stream_cell::canonical_to_display(const QPointF& point_pct) const {
    const QPointF canonical = clamp_pct(point_pct);
    QPointF display_pct;
    switch (quarter_turns_) {
    case 1:
        display_pct = { 100.0 - canonical.y(), canonical.x() };
        break;
    case 2:
        display_pct = { 100.0 - canonical.x(), 100.0 - canonical.y() };
        break;
    case 3:
        display_pct = { canonical.y(), 100.0 - canonical.x() };
        break;
    default:
        display_pct = canonical;
        break;
    }

    const QRectF frame_rect = display_frame_rect();
    return {
        frame_rect.left() + display_pct.x() / 100.0 * frame_rect.width(),
        frame_rect.top() + display_pct.y() / 100.0 * frame_rect.height(),
    };
}

QPointF stream_cell::display_to_canonical(const QPointF& point_px) const {
    const QRectF frame_rect = display_frame_rect();
    if (!(frame_rect.width() > 0.0) || !(frame_rect.height() > 0.0)) {
        return {};
    }

    const QPointF display_pct = clamp_pct(
        {
            (point_px.x() - frame_rect.left()) / frame_rect.width() * 100.0,
            (point_px.y() - frame_rect.top()) / frame_rect.height() * 100.0,
        }
    );

    switch (quarter_turns_) {
    case 1:
        return { display_pct.y(), 100.0 - display_pct.x() };
    case 2:
        return { 100.0 - display_pct.x(), 100.0 - display_pct.y() };
    case 3:
        return { 100.0 - display_pct.y(), display_pct.x() };
    default:
        return display_pct;
    }
}

std::vector<QPointF> stream_cell::display_points_pct(
    const std::vector<QPointF>& canonical_points
) const {
    std::vector<QPointF> result;
    result.reserve(canonical_points.size());
    for (const QPointF& canonical : canonical_points) {
        switch (quarter_turns_) {
        case 1:
            result.emplace_back(100.0 - canonical.y(), canonical.x());
            break;
        case 2:
            result.emplace_back(100.0 - canonical.x(), 100.0 - canonical.y());
            break;
        case 3:
            result.emplace_back(canonical.y(), 100.0 - canonical.x());
            break;
        default:
            result.push_back(canonical);
            break;
        }
    }
    return result;
}

QPointF stream_cell::to_pct(const QPointF& pos_px) const {
    return display_to_canonical(pos_px);
}

QPointF stream_cell::to_px(const QPointF& pos_pct) const {
    return canonical_to_display(pos_pct);
}
