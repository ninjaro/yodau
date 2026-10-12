#include "widgets/line_wave_preview.hpp"

#include "shell/str_label.hpp"
#include "widgets/line_wave_renderer.hpp"

#include <QPainter>

line_wave_preview::line_wave_preview(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("line_wave_preview"));
    setAccessibleName(str_label("Synthetic string wave preview"));
    setAccessibleDescription(str_label(
        "One fixed test hit on a sample path, not live stream geometry or "
        "audio. "
        "Use the sample-age slider to inspect travel and decay."
    ));
    setMinimumSize(180, 130);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void line_wave_preview::set_sample(
    const line_profile& profile, const int age_ms, const double display_gain
) {
    profile_ = profile;
    age_ms_ = std::clamp(age_ms, 0, int(line_wave_renderer::pulse_ttl_ms));
    display_gain_ = display_gain;
    update();
}

void line_wave_preview::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), palette().base());
    painter.setClipRect(rect().adjusted(1, 1, -1, -1));

    const std::vector<QPointF> points = profile_.closed
        ? std::vector<QPointF> { { 15, 30 },
                                 { 85, 30 },
                                 { 85, 70 },
                                 { 15, 70 } }
        : std::vector<QPointF> { { 10, 50 }, { 90, 50 } };
    const auto geometry
        = line_wave_renderer::build_path_geometry(points, profile_.closed);
    const QPointF hit(50, profile_.closed ? 30 : 50);
    const auto projection = line_wave_renderer::project_hit(geometry, hit);
    if (!projection) {
        return;
    }

    painter.setPen(QPen(palette().mid().color(), 1, Qt::DashLine));
    for (const auto& segment : geometry.segments) {
        painter.drawLine(
            line_wave_renderer::to_px(segment.a_pct, size()),
            line_wave_renderer::to_px(segment.b_pct, size())
        );
    }

    // Automatic stream colours require a real frame/index. Use an honest
    // representative palette colour, not a fabricated negative-camera colour.
    const QColor color = profile_.color_mode_id == QStringLiteral("manual")
        ? profile_.color
        : palette().highlight().color();
    QVector<line_wave_renderer::pulse> pulses;
    line_wave_renderer::add_pulses(
        pulses, *projection, 0, 0.8, QStringLiteral("neg_to_pos"), 0.8,
        profile_.width_text, profile_.length_text, profile_.response_text
    );
    line_wave_renderer::draw(
        painter, geometry, pulses, size(), color, profile_.width_text,
        profile_.length_text, profile_.response_text, age_ms_, display_gain_
    );
    painter.setPen(palette().text().color());
    painter.drawEllipse(line_wave_renderer::to_px(hit, size()), 3, 3);
}
