#ifndef YODAU_APP_WIDGETS_PROCESSING_OVERLAY_HPP
#define YODAU_APP_WIDGETS_PROCESSING_OVERLAY_HPP

#include <QPointF>
#include <QString>

#include <optional>
#include <vector>

enum class processing_overlay_kind { point, polyline, polygon, label };

struct processing_overlay_instance {
    processing_overlay_kind kind { processing_overlay_kind::point };
    QString label;
    std::vector<QPointF> points_pct;
    std::optional<QPointF> anchor_pct;
};

#endif // YODAU_APP_WIDGETS_PROCESSING_OVERLAY_HPP
