#ifndef YODAU_APP_WIDGETS_PROCESSING_OVERLAY_RENDERER_HPP
#define YODAU_APP_WIDGETS_PROCESSING_OVERLAY_RENDERER_HPP

#include "shell/app_settings.hpp"
#include "widgets/processing_overlay.hpp"

#include <QRectF>
#include <QString>

#include <vector>

class QPainter;

namespace processing_overlay_renderer {

QString selected_movement_mode(const stream_settings& settings_value);
QString resolved_backend_overlay_mode(const stream_settings& settings_value);
QString resolved_draw_overlay_mode(
    const stream_settings& settings_value,
    const std::vector<processing_overlay_instance>& overlays
);
bool event_bubbles_visible(
    const stream_settings& settings_value,
    const std::vector<processing_overlay_instance>& overlays
);
bool tripwire_waves_visible(const stream_settings& settings_value);

void draw(
    QPainter& painter, const QRectF& bounds,
    const stream_settings& settings_value,
    const std::vector<processing_overlay_instance>& overlays
);

} // namespace processing_overlay_renderer

#endif // YODAU_APP_WIDGETS_PROCESSING_OVERLAY_RENDERER_HPP
