#include "widgets/stream_cell.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineF>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QFontMetrics>
#include <QPen>
#include <QPolygonF>
#include <QPushButton>
#include <QStyle>
#include <QStyleOption>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

#include "shell/icon_loader.hpp"

namespace stream_cell_support {

constexpr int wave_animation_interval_ms = 16;
constexpr qint64 wave_pulse_ttl_ms = 2400;
constexpr qint64 wave_merge_window_ms = 120;
constexpr double wave_joint_transfer = 0.72;

struct path_segment {
    int index { -1 };
    QPointF a_pct;
    QPointF b_pct;
    double start_pos { 0.0 };
    double length { 0.0 };
};

struct path_geometry {
    bool closed { false };
    double total_length { 0.0 };
    std::vector<path_segment> segments;
};

struct hit_projection {
    int segment_index { -1 };
    double path_pos { 0.0 };
};

double clamp_unit(double value) {
    return std::clamp(value, 0.0, 1.0);
}

QColor color_with_alpha(QColor color, const int alpha) {
    color.setAlpha(std::clamp(alpha, 0, 255));
    return color;
}

QColor log_mode_badge_color(const frontend_log_mode mode) {
    return mode == frontend_log_mode::debug
        ? QColor(QStringLiteral("#bc6c25"))
        : QColor(QStringLiteral("#3a5a40"));
}

struct line_wave_style {
    double amplitude_k { 1.0 };
    double speed_k { 1.0 };
    double spread_k { 1.0 };
    double frequency_k { 1.0 };
    double damping_k { 1.0 };
    double glow_width_k { 1.0 };
};

double normalized_numeric_value(const QString& text) {
    bool ok = false;
    const double value = text.toDouble(&ok);
    if (!ok || value <= 0.0) {
        return -1.0;
    }

    return value;
}

double line_length_scale(const QString& length_text) {
    const QString normalized = normalized_line_length_text(length_text);

    if (normalized == QStringLiteral("short")) {
        return 0.8;
    }
    if (normalized == QStringLiteral("long")) {
        return 1.35;
    }

    const double numeric_value = normalized_numeric_value(normalized);
    if (numeric_value > 0.0) {
        return std::clamp(numeric_value, 0.6, 1.8);
    }

    return 1.0;
}

double line_response_scale(const QString& response_text) {
    const QString normalized = normalized_line_response_text(response_text);

    if (normalized == QStringLiteral("dry")) {
        return 0.82;
    }
    if (normalized == QStringLiteral("resonant")) {
        return 1.35;
    }

    const double numeric_value = normalized_numeric_value(normalized);
    if (numeric_value > 0.0) {
        return std::clamp(numeric_value, 0.6, 1.8);
    }

    return 1.0;
}

line_wave_style line_wave_style_for_line(
    const stream_cell::line_instance& line_value
) {
    const double width_scale = std::clamp(
        static_cast<double>(line_width_visual_value(line_value.width_text))
            / 3.0,
        0.6, 2.2
    );
    const double length_scale = line_length_scale(line_value.length_text);
    const double response_scale = line_response_scale(line_value.response_text);

    line_wave_style style;
    style.amplitude_k = std::clamp(
        0.74 + width_scale * 0.24 + (response_scale - 1.0) * 0.28,
        0.55, 1.85
    );
    style.speed_k = std::clamp(
        1.08 - (width_scale - 1.0) * 0.18 - (length_scale - 1.0) * 0.40,
        0.55, 1.45
    );
    style.spread_k = std::clamp(
        0.84 + width_scale * 0.18 + length_scale * 0.20
            + (response_scale - 1.0) * 0.18,
        0.65, 1.95
    );
    style.frequency_k = std::clamp(
        1.16 - (width_scale - 1.0) * 0.16 - (length_scale - 1.0) * 0.42,
        0.5, 1.65
    );
    style.damping_k = std::clamp(
        1.08 - (width_scale - 1.0) * 0.08 - (length_scale - 1.0) * 0.16
            - (response_scale - 1.0) * 0.48,
        0.45, 1.55
    );
    style.glow_width_k = std::clamp(
        0.9 + width_scale * 0.16 + (response_scale - 1.0) * 0.14,
        0.75, 1.55
    );
    return style;
}

double event_region_scale(const stream_settings& settings_value) {
    const QString algorithm_id = normalized_frontend_algorithm_id(
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

    if (algorithm_id == QStringLiteral("contour_mask")) {
        if (preset_id == QStringLiteral("outline")) {
            return 0.14;
        }
        if (preset_id == QStringLiteral("mask_heavy")) {
            return 0.22;
        }
        return 0.18;
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

QRectF overlay_region_rect(
    const QRect& rect_value, const QPointF& center, const stream_settings& settings_value
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

void draw_baseline_overlay(
    QPainter& painter, const QPointF& center, const double radius,
    const QColor& color, const stream_settings& settings_value, const double life_k
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

void draw_spot_grid_overlay(
    QPainter& painter, const QRectF& region, const QPointF& center,
    const double radius, const QColor& color, const stream_settings& settings_value,
    const double life_k
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

void draw_contour_mask_overlay(
    QPainter& painter, const QRectF& region, const QPointF& center,
    const double radius, const QColor& color, const stream_settings& settings_value,
    const double life_k
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

void draw_algorithm_overlay(
    QPainter& painter, const QRect& rect_value, const QPointF& center,
    const double radius, const QColor& color, const stream_settings& settings_value,
    const double life_k
) {
    const QString algorithm_id = normalized_frontend_algorithm_id(
        settings_value.algorithm_id
    );

    if (algorithm_id == QStringLiteral("spot_grid")) {
        draw_spot_grid_overlay(
            painter, overlay_region_rect(rect_value, center, settings_value),
            center, radius, color, settings_value, life_k
        );
        return;
    }

    if (algorithm_id == QStringLiteral("contour_mask")) {
        draw_contour_mask_overlay(
            painter, overlay_region_rect(rect_value, center, settings_value),
            center, radius, color, settings_value, life_k
        );
        return;
    }

    draw_baseline_overlay(
        painter, center, radius, color, settings_value, life_k
    );
}

double point_distance_pct(const QPointF& a_pct, const QPointF& b_pct) {
    const double dx = b_pct.x() - a_pct.x();
    const double dy = b_pct.y() - a_pct.y();
    return std::sqrt(dx * dx + dy * dy);
}

path_geometry build_path_geometry(const stream_cell::line_instance& line_value) {
    path_geometry geometry;
    geometry.closed = line_value.closed;

    if (line_value.pts_pct.size() < 2) {
        return geometry;
    }

    geometry.segments.reserve(
        line_value.pts_pct.size() + (line_value.closed ? 1u : 0u)
    );

    for (size_t i = 1; i < line_value.pts_pct.size(); ++i) {
        const QPointF a_pct = line_value.pts_pct[i - 1];
        const QPointF b_pct = line_value.pts_pct[i];
        const double length = point_distance_pct(a_pct, b_pct);

        if (length <= 0.0) {
            continue;
        }

        geometry.segments.push_back(
            path_segment {
                .index = static_cast<int>(geometry.segments.size()),
                .a_pct = a_pct,
                .b_pct = b_pct,
                .start_pos = geometry.total_length,
                .length = length,
            }
        );
        geometry.total_length += length;
    }

    if (line_value.closed && line_value.pts_pct.size() > 2) {
        const QPointF a_pct = line_value.pts_pct.back();
        const QPointF b_pct = line_value.pts_pct.front();
        const double length = point_distance_pct(a_pct, b_pct);

        if (length > 0.0) {
            geometry.segments.push_back(
                path_segment {
                    .index = static_cast<int>(geometry.segments.size()),
                    .a_pct = a_pct,
                    .b_pct = b_pct,
                    .start_pos = geometry.total_length,
                    .length = length,
                }
            );
            geometry.total_length += length;
        }
    }

    return geometry;
}

std::optional<hit_projection>
project_hit(const path_geometry& geometry, const QPointF& hit_pct) {
    if (geometry.segments.empty()) {
        return {};
    }

    hit_projection best_projection;
    double best_dist2 = std::numeric_limits<double>::max();

    for (const auto& segment : geometry.segments) {
        const double abx = segment.b_pct.x() - segment.a_pct.x();
        const double aby = segment.b_pct.y() - segment.a_pct.y();
        const double len2 = abx * abx + aby * aby;
        if (len2 <= 0.0) {
            continue;
        }

        const double apx = hit_pct.x() - segment.a_pct.x();
        const double apy = hit_pct.y() - segment.a_pct.y();
        double t = (apx * abx + apy * aby) / len2;
        t = std::clamp(t, 0.0, 1.0);

        const double proj_x = segment.a_pct.x() + t * abx;
        const double proj_y = segment.a_pct.y() + t * aby;
        const double dx = hit_pct.x() - proj_x;
        const double dy = hit_pct.y() - proj_y;
        const double dist2 = dx * dx + dy * dy;

        if (dist2 >= best_dist2) {
            continue;
        }

        best_dist2 = dist2;
        best_projection.segment_index = segment.index;
        best_projection.path_pos = segment.start_pos + t * segment.length;
    }

    if (best_projection.segment_index < 0) {
        return {};
    }

    return best_projection;
}

int segment_hops(
    const path_geometry& geometry, const int lhs_index, const int rhs_index
) {
    int hops = std::abs(lhs_index - rhs_index);
    if (!geometry.closed) {
        return hops;
    }

    const int segment_count = static_cast<int>(geometry.segments.size());
    if (segment_count <= 0) {
        return hops;
    }

    return std::min(hops, segment_count - hops);
}

double forward_distance(
    const path_geometry& geometry, const double origin_pos,
    const double sample_pos, const int travel_sign
) {
    if (!geometry.closed) {
        return travel_sign > 0 ? sample_pos - origin_pos : origin_pos - sample_pos;
    }

    if (geometry.total_length <= 0.0) {
        return 0.0;
    }

    double delta
        = travel_sign > 0 ? sample_pos - origin_pos : origin_pos - sample_pos;

    while (delta < 0.0) {
        delta += geometry.total_length;
    }
    while (delta >= geometry.total_length) {
        delta -= geometry.total_length;
    }

    return delta;
}

double line_wave_displacement_px(
    const path_geometry& geometry, const int segment_index,
    const double sample_path_pos, const qint64 now_ms,
    const QVector<stream_cell::line_wave_pulse>& pulses
) {
    double displacement = 0.0;

    for (const auto& pulse : pulses) {
        const qint64 age_ms = now_ms - pulse.start_ms;
        if (age_ms < 0 || age_ms >= wave_pulse_ttl_ms) {
            continue;
        }

        const double age_s = static_cast<double>(age_ms) / 1000.0;
        const double travel = pulse.speed * age_s;
        const double sample_progress = forward_distance(
            geometry, pulse.origin_path_pos, sample_path_pos, pulse.travel_sign
        );

        if (!geometry.closed && sample_progress < 0.0) {
            continue;
        }

        const double spread = pulse.spread * (1.0 + age_s * 0.2);
        if (spread <= 0.0) {
            continue;
        }

        const double distance_to_front = std::abs(sample_progress - travel);
        const double envelope = std::exp(
            -(distance_to_front * distance_to_front) / (2.0 * spread * spread)
        );
        if (envelope < 0.002) {
            continue;
        }

        const int hops = segment_hops(
            geometry, pulse.source_segment_index, segment_index
        );
        const double joint_factor = std::pow(
            wave_joint_transfer, static_cast<double>(hops)
        );
        const double time_decay = std::exp(-pulse.damping_per_s * age_s);

        double edge_factor = 1.0;
        if (!geometry.closed && geometry.total_length > 0.0) {
            const double edge_band
                = std::clamp(geometry.total_length * 0.18, 8.0, 24.0);
            const double edge_pos = std::min(
                sample_path_pos, geometry.total_length - sample_path_pos
            );
            edge_factor = 0.35 + 0.65 * clamp_unit(edge_pos / edge_band);
        }

        const double phase
            = 2.0 * std::numbers::pi * pulse.frequency_hz * age_s
            - sample_progress * 0.35;

        displacement += pulse.displacement_sign * pulse.amplitude
            * joint_factor * time_decay * envelope * edge_factor
            * std::sin(phase);
    }

    return displacement;
}

} // namespace stream_cell_support

stream_cell::stream_cell(const QString& stream_name, QWidget* parent)
    : QWidget(parent)
    , name(stream_name)
    , close_btn(nullptr)
    , focus_btn(nullptr)
    , name_label(nullptr)
    , player(nullptr)
    , sink(nullptr)
    , camera(nullptr)
    , session(nullptr) {
    stream_settings_value.stream_name = stream_name;
    stream_settings_value.algorithm_id = default_frontend_algorithm_id();
    stream_settings_value.algorithm_preset = default_algorithm_preset_id(
        stream_settings_value.algorithm_id
    );
    build_ui();
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    repaint_timer.start();
    animation_clock.start();
}

const QString& stream_cell::get_name() const { return name; }

bool stream_cell::is_active() const { return active; }

std::vector<QPointF> stream_cell::draft_points_pct() const {
    return draft_line_points_pct;
}

bool stream_cell::draft_closed() const { return draft_line_closed; }

QString stream_cell::draft_name() const { return draft_line_name; }

QColor stream_cell::draft_color() const { return draft_line_color; }

bool stream_cell::is_draft_preview() const { return draft_preview; }

stream_settings stream_cell::current_stream_settings() const {
    return stream_settings_value;
}

stream_runtime_metrics stream_cell::current_runtime_metrics() const {
    return runtime_metrics_value;
}

frontend_log_mode stream_cell::current_log_mode() const {
    return log_mode_value;
}

void stream_cell::set_active(const bool val) {
    if (active == val) {
        return;
    }
    active = val;
    if (!active) {
        set_drawing_enabled(false);
        clear_draft();
    }
    update_icon();
    update();
}

void stream_cell::set_drawing_enabled(const bool on) {
    drawing_enabled = on;
    if (!on) {
        hover_point_pct.reset();
    }
    setMouseTracking(drawing_enabled);
    update();
}

void stream_cell::set_stream_settings(const stream_settings& settings_value) {
    stream_settings_value = settings_value;
    if (stream_settings_value.stream_name.trimmed().isEmpty()) {
        stream_settings_value.stream_name = name;
    }
    stream_settings_value.algorithm_id = normalized_frontend_algorithm_id(
        stream_settings_value.algorithm_id
    );
    stream_settings_value.algorithm_preset = normalized_algorithm_preset_id(
        stream_settings_value.algorithm_id,
        stream_settings_value.algorithm_preset
    );
    update();
}

void stream_cell::set_runtime_metrics(const stream_runtime_metrics& metrics) {
    runtime_metrics_value = metrics;
    update();
}

void stream_cell::set_log_mode(const frontend_log_mode mode) {
    if (log_mode_value == mode) {
        return;
    }

    log_mode_value = mode;
    update();
}

void stream_cell::set_draft_params(
    const QString& n, const QColor& color, const bool closed,
    const QString& width_text, const QString& length_text,
    const QString& response_text
) {
    draft_line_name = n;
    draft_line_color = color;
    draft_line_closed = closed;
    draft_line_width_text = normalized_line_width_text(width_text);
    draft_line_length_text = normalized_line_length_text(length_text);
    draft_line_response_text = normalized_line_response_text(response_text);
    update();
}

void stream_cell::set_draft_points_pct(const std::vector<QPointF>& pts) {
    draft_line_points_pct = pts;
    update();
}

void stream_cell::clear_draft() {
    draft_line_points_pct.clear();
    hover_point_pct.reset();
    draft_preview = false;
    draft_line_width_text = default_line_width_text();
    draft_line_length_text = default_line_length_text();
    draft_line_response_text = default_line_response_text();
    update();
}

void stream_cell::set_persistent_lines(
    const std::vector<line_instance>& lines
) {
    persistent_lines = lines;
    line_highlights.clear();
    line_hits.clear();
    line_waves.clear();
    update_animation_timer();
    update();
}

void stream_cell::add_persistent_line(const line_instance& line) {
    persistent_lines.push_back(line);
    update();
}

void stream_cell::clear_persistent_lines() {
    persistent_lines.clear();
    line_highlights.clear();
    line_hits.clear();
    line_waves.clear();
    update_animation_timer();
    update();
}

void stream_cell::set_draft_preview(const bool on) {
    draft_preview = on;
    update();
}

void stream_cell::set_labels_enabled(const bool on) {
    if (labels_enabled == on) {
        return;
    }
    labels_enabled = on;
    update();
}

void stream_cell::set_source(const QUrl& source) {
    if (!player) {
        return;
    }

    last_error.clear();
    last_frame = QImage();

    player->setSource(source);
    player->play();
}

void stream_cell::set_loop(const bool on) { loop_enabled = on; }

void stream_cell::set_camera_id(const QByteArray& id) {
    camera_id = id;

    last_error.clear();
    last_frame = QImage();

    if (player) {
        player->stop();
    }

    if (camera) {
        camera->stop();
        camera->deleteLater();
        camera = nullptr;
    }

    if (!session) {
        session = new QMediaCaptureSession(this);
        session->setVideoSink(sink);
    }

    QCameraDevice device;
    const auto cams = QMediaDevices::videoInputs();
    for (const auto& c : cams) {
        if (c.id() == id) {
            device = c;
            break;
        }
    }

    if (device.isNull()) {
        last_error = tr("camera not found");
        update();
        return;
    }

    camera = new QCamera(device, this);
    session->setCamera(camera);

    connect(
        camera, &QCamera::errorOccurred, this, &stream_cell::on_camera_error
    );

    camera->start();
}

void stream_cell::add_event(const QPointF& pos_pct, const QColor& color) {
    event_instance e;
    e.pos_pct = pos_pct;
    e.color = color;
    e.ts = QDateTime::currentDateTime();

    events.push_back(e);
    update();
}

void stream_cell::set_repaint_interval_ms(const int ms) {
    if (ms <= 0) {
        return;
    }
    repaint_interval_ms = ms;
}

void stream_cell::highlight_line(const QString& line_name) {
    if (line_name.isEmpty()) {
        return;
    }
    line_highlights[line_name] = QDateTime::currentDateTime();
    update();
}

void stream_cell::highlight_line_at(
    const QString& line_name, const QPointF& pos_pct, double strength,
    const QString& direction, double speed
) {
    if (line_name.isEmpty()) {
        return;
    }

    hit_info h;
    h.pos_pct = pos_pct;
    h.ts = QDateTime::currentDateTime();
    h.strength = strength;

    auto& hits = line_hits[line_name];

    for (int i = 0; i < hits.size(); i += 1) {
        const int age = static_cast<int>(hits[i].ts.msecsTo(h.ts));
        if (age >= line_highlight_ttl_ms) {
            hits.removeAt(i);
            i -= 1;
        }
    }

    hits.push_back(h);

    line_highlights[line_name] = h.ts;
    add_line_wave(line_name, pos_pct, strength, direction, speed);
    update();
}

void stream_cell::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QStyleOption opt;
    opt.initFrom(this);

    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);

    if (!last_frame.isNull()) {
        p.drawImage(rect(), last_frame);
    } else {
        const QString txt = last_error.isEmpty() ? "no signal" : last_error;
        const QRect r = rect().adjusted(6, 6, -6, -6);
        p.setPen(palette().color(QPalette::Text));
        p.drawText(r, Qt::AlignCenter, txt);
    }

    p.drawRect(rect().adjusted(0, 0, -1, -1));
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto now = QDateTime::currentDateTime();

    for (auto it = line_highlights.begin(); it != line_highlights.end();) {
        const int age = static_cast<int>(it.value().msecsTo(now));
        if (age >= line_highlight_ttl_ms) {
            it = line_highlights.erase(it);
        } else {
            ++it;
        }
    }

    prune_line_waves(animation_clock.elapsed());
    update_animation_timer();

    draw_events(p);
    draw_persistent(p);
    draw_draft(p);
    draw_hover_point(p);
    draw_hover_coords(p);
    draw_preview_segment(p);
    draw_stream_name(p);
    draw_runtime_metrics(p);
}

void stream_cell::mousePressEvent(QMouseEvent* event) {
    if (!drawing_enabled || !active) {
        QWidget::mousePressEvent(event);
        return;
    }

    auto* child = childAt(event->pos());
    if (child == close_btn || child == focus_btn) {
        QWidget::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        setFocus();
        draft_line_points_pct.push_back(to_pct(event->pos()));
        update();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void stream_cell::mouseMoveEvent(QMouseEvent* event) {
    if (!drawing_enabled || !active) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    hover_point_pct = to_pct(event->pos());
    update();
    event->accept();
}

void stream_cell::leaveEvent(QEvent* event) {
    hover_point_pct.reset();
    update();
    QWidget::leaveEvent(event);
}

void stream_cell::keyPressEvent(QKeyEvent* event) {
    if (!(drawing_enabled && active)) {
        QWidget::keyPressEvent(event);
        return;
    }

    const bool undo_key = (event->key() == Qt::Key_Backspace)
        || (event->key() == Qt::Key_Z
            && (event->modifiers() & Qt::ControlModifier));

    if (!undo_key) {
        QWidget::keyPressEvent(event);
        return;
    }

    if (!draft_line_points_pct.empty()) {
        draft_line_points_pct.pop_back();
        hover_point_pct.reset();
        update();
    }

    event->accept();
}

void stream_cell::build_ui() {
    const auto root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(6);

    const auto top_row = new QHBoxLayout();
    top_row->setContentsMargins(0, 0, 0, 0);
    top_row->setSpacing(4);

    top_row->addStretch();

    focus_btn = new QPushButton(this);
    focus_btn->setFixedSize(24, 24);
    focus_btn->setIconSize(QSize(16, 16));
    focus_btn->setFlat(true);
    focus_btn->setFocusPolicy(Qt::NoFocus);
    update_icon();
    top_row->addWidget(focus_btn);

    close_btn = new QPushButton(this);
    close_btn->setFixedSize(24, 24);
    close_btn->setIconSize(QSize(16, 16));
    close_btn->setToolTip(tr("close"));
    close_btn->setFlat(true);
    close_btn->setFocusPolicy(Qt::NoFocus);
    close_btn->setIcon(icon_loader::title_bar_close_icon());
    top_row->addWidget(close_btn);
    root->addLayout(top_row);
    root->addStretch(1);

    sink = new QVideoSink(this);
    connect(
        sink, &QVideoSink::videoFrameChanged, this,
        &stream_cell::on_frame_changed
    );

    player = new QMediaPlayer(this);
    player->setVideoOutput(sink);
    animation_timer = new QTimer(this);
    animation_timer->setInterval(stream_cell_support::wave_animation_interval_ms);
    // player->setSource(QUrl::fromLocalFile("/home/yarro/Pictures/kino/1080.mp4"));
    // player->play();

    connect(
        close_btn, &QPushButton::clicked, this, &stream_cell::on_close_clicked
    );
    connect(
        focus_btn, &QPushButton::clicked, this, &stream_cell::on_focus_clicked
    );
    connect(
        player, &QMediaPlayer::mediaStatusChanged, this,
        &stream_cell::on_media_status_changed
    );
    connect(
        player, &QMediaPlayer::errorOccurred, this,
        &stream_cell::on_player_error
    );
    connect(
        animation_timer, &QTimer::timeout, this, &stream_cell::on_animation_tick
    );
}

void stream_cell::update_icon() {
    if (!focus_btn) {
        return;
    }
    if (active) {
        focus_btn->setToolTip(tr("shrink"));
        focus_btn->setIcon(icon_loader::title_bar_restore_icon());
    } else {
        focus_btn->setToolTip(tr("enlarge"));
        focus_btn->setIcon(icon_loader::title_bar_maximize_icon());
    }
}

void stream_cell::draw_poly_with_points(
    QPainter& p, const std::vector<QPointF>& pts_pct, const QColor& color,
    bool closed, Qt::PenStyle style, qreal width
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

    for (const auto& pt_px : poly) {
        p.drawEllipse(pt_px, 3.0, 3.0);
    }
}

void stream_cell::draw_persistent(QPainter& p) const {
    const auto now = QDateTime::currentDateTime();

    for (const auto& l : persistent_lines) {
        const auto key = l.template_name.trimmed();

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
                    QColor hc = l.color;
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

                        QColor hc = l.color;
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
                            QColor hc = l.color;
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
            p, l.pts_pct, l.color, l.closed, Qt::SolidLine,
            line_width_visual_value(l.width_text)
        );
        draw_wave_overlay(p, l);

        if (!(active && labels_enabled)) {
            continue;
        }

        const auto text = key;
        if (text.isEmpty()) {
            continue;
        }

        p.setPen(l.color);
        p.drawText(label_pos_px(l), text);
    }
}

void stream_cell::draw_draft(QPainter& p) const {
    if (draft_line_points_pct.empty()) {
        return;
    }

    draw_poly_with_points(
        p, draft_line_points_pct, draft_line_color, draft_line_closed,
        Qt::DashLine, line_width_visual_value(draft_line_width_text)
    );
}

void stream_cell::draw_hover_point(QPainter& p) const {
    if (!hover_point_pct.has_value()) {
        return;
    }

    QPen hpen(draft_line_color);
    hpen.setWidthF(1.0);
    hpen.setStyle(Qt::DashLine);
    p.setPen(hpen);

    p.drawEllipse(to_px(*hover_point_pct), 4.0, 4.0);
}

void stream_cell::draw_hover_coords(QPainter& p) const {
    if (!(hover_point_pct.has_value() && drawing_enabled && active)) {
        return;
    }

    const auto& hp = *hover_point_pct;
    QString txt
        = QString("x=%1  y=%2").arg(hp.x(), 0, 'f', 1).arg(hp.y(), 0, 'f', 1);

    QRect r = rect().adjusted(6, 6, -6, -6);
    p.setPen(palette().color(QPalette::Text));
    p.drawText(r, Qt::AlignLeft | Qt::AlignBottom, txt);
}

void stream_cell::draw_preview_segment(QPainter& p) const {
    if (!(drawing_enabled && active && hover_point_pct.has_value()
          && !draft_line_points_pct.empty())) {
        return;
    }

    QPen pen(draft_line_color);
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

void stream_cell::draw_stream_name(QPainter& p) const {
    if (name.isEmpty()) {
        return;
    }

    QRect r = rect().adjusted(6, 6, -6, -6);
    p.setPen(palette().color(QPalette::Text));
    p.drawText(r, Qt::AlignLeft | Qt::AlignTop, name);

    const QString badge_text = algorithm_badge_text(
        stream_settings_value.algorithm_id,
        stream_settings_value.algorithm_preset,
        stream_settings_value.algorithm_overlay_enabled
    );
    const QColor badge_color = algorithm_badge_color(
        stream_settings_value.algorithm_id
    );
    const QString log_badge_text = QStringLiteral("log %1").arg(
        frontend_log_mode_name(log_mode_value)
    );
    const QColor log_badge_fill = stream_cell_support::log_mode_badge_color(
        log_mode_value
    );

    QFont badge_font = p.font();
    const double badge_point_size = badge_font.pointSizeF() > 0.0
        ? badge_font.pointSizeF()
        : 9.0;
    badge_font.setPointSizeF(std::max(8.0, badge_point_size - 0.5));
    p.setFont(badge_font);

    const QFontMetrics badge_metrics(badge_font);
    QRect badge_rect = badge_metrics.boundingRect(badge_text);
    badge_rect.adjust(-8, -4, 8, 4);
    badge_rect.moveBottomRight(QPoint(r.right(), r.bottom()));

    QRect log_badge_rect = badge_metrics.boundingRect(log_badge_text);
    log_badge_rect.adjust(-8, -4, 8, 4);
    log_badge_rect.moveBottomRight(
        QPoint(r.right(), badge_rect.top() - 6)
    );

    QColor badge_fill = badge_color;
    badge_fill.setAlpha(active ? 208 : 176);
    p.setPen(Qt::NoPen);
    p.setBrush(
        stream_cell_support::color_with_alpha(
            log_badge_fill, active ? 188 : 160
        )
    );
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
    const double summary_point_size = summary_font.pointSizeF() > 0.0
        ? summary_font.pointSizeF()
        : 9.0;
    summary_font.setPointSizeF(std::max(8.0, summary_point_size - 1.0));
    p.setFont(summary_font);
    p.setPen(palette().color(QPalette::Text));

    const QString summary = algorithm_summary_text(
        stream_settings_value.algorithm_id,
        stream_settings_value.algorithm_preset,
        stream_settings_value.algorithm_overlay_enabled
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
    const QString metrics_text = stream_runtime_metrics_text(runtime_metrics_value);
    if (metrics_text.trimmed().isEmpty()) {
        return;
    }

    QFont metrics_font = p.font();
    const double metrics_point_size = metrics_font.pointSizeF() > 0.0
        ? metrics_font.pointSizeF()
        : 9.0;
    metrics_font.setPointSizeF(std::max(7.5, metrics_point_size - 1.0));
    p.setFont(metrics_font);

    const QRect bounds = rect().adjusted(6, 6, -6, -6);
    const QFontMetrics metrics(metrics_font);
    const QString rendered_text = (!active && bounds.width() < 220)
        ? QStringLiteral(
              "v %1 | b %2"
          )
              .arg(
                  runtime_metrics_value.input_fps > 0.0
                      ? QString::number(runtime_metrics_value.input_fps, 'f', 1)
                      : QStringLiteral("--")
              )
              .arg(
                  runtime_metrics_value.backend_fps > 0.0
                      ? QString::number(runtime_metrics_value.backend_fps, 'f', 1)
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

QPointF stream_cell::label_pos_px(const line_instance& l) const {
    if (l.pts_pct.empty()) {
        return {};
    }

    const QPointF anchor_pct = l.closed ? l.pts_pct.back() : l.pts_pct.front();
    const QPointF anchor_px = to_px(anchor_pct);
    return { anchor_px.x() + 6.0, anchor_px.y() + 14.0 };
}

QPointF stream_cell::to_pct(const QPointF& pos_px) const {
    if (width() <= 0 || height() <= 0) {
        return {};
    }

    float x
        = static_cast<float>(pos_px.x()) / static_cast<float>(width()) * 100.0f;
    float y = static_cast<float>(pos_px.y()) / static_cast<float>(height())
        * 100.0f;

    x = std::clamp(x, 0.f, 100.f);
    y = std::clamp(y, 0.f, 100.f);

    return { x, y };
}

QPointF stream_cell::to_px(const QPointF& pos_pct) const {
    return { pos_pct.x() / 100.0 * width(), pos_pct.y() / 100.0 * height() };
}

void stream_cell::draw_events(QPainter& p) {
    const auto now = QDateTime::currentDateTime();
    const int ttl_ms = 2000;

    const QRect r = rect();
    const double w = static_cast<double>(r.width());
    const double h = static_cast<double>(r.height());
    const double base = std::min(w, h);
    const double radius = base * 0.015;

    QVector<event_instance> alive;
    alive.reserve(events.size());

    for (const auto& e : events) {
        const int age = static_cast<int>(e.ts.msecsTo(now));
        if (age >= ttl_ms) {
            continue;
        }

        alive.push_back(e);

        const double k = 1.0 - static_cast<double>(age) / ttl_ms;
        int a = static_cast<int>(120.0 * k);
        if (a < 0) {
            a = 0;
        }

        QColor c = e.color;
        c.setAlpha(a);

        const double x = r.left() + w * (e.pos_pct.x() / 100.0);
        const double y = r.top() + h * (e.pos_pct.y() / 100.0);
        const QPointF center(x, y);

        if (stream_settings_value.algorithm_overlay_enabled) {
            stream_cell_support::draw_algorithm_overlay(
                p, r, center, radius, c, stream_settings_value, k
            );
        }

        p.setPen(Qt::NoPen);
        p.setBrush(
            stream_cell_support::color_with_alpha(
                c,
                stream_settings_value.algorithm_overlay_enabled
                    ? static_cast<int>(210.0 * k)
                    : a
            )
        );
        p.drawEllipse(
            center,
            stream_settings_value.algorithm_overlay_enabled
                ? radius * 0.55
                : radius,
            stream_settings_value.algorithm_overlay_enabled
                ? radius * 0.55
                : radius
        );
    }

    events = std::move(alive);
}

double stream_cell::segment_impact_k(
    const QPointF& a_pct, const QPointF& b_pct,
    const QVector<hit_info>& hit_points, double falloff_pct, double ktime
) const {
    const double abx = b_pct.x() - a_pct.x();
    const double aby = b_pct.y() - a_pct.y();
    const double ab_len2 = abx * abx + aby * aby;

    if (ab_len2 <= 0.0) {
        return 0.0;
    }

    double best_impact = 0.0;

    for (int j = 0; j < hit_points.size(); j += 1) {
        const hit_info hp_info = hit_points[j];
        const QPointF hp = hp_info.pos_pct;

        const double apx = hp.x() - a_pct.x();
        const double apy = hp.y() - a_pct.y();

        double t = (apx * abx + apy * aby) / ab_len2;
        if (t < 0.0) {
            t = 0.0;
        } else if (t > 1.0) {
            t = 1.0;
        }

        const double projx = a_pct.x() + t * abx;
        const double projy = a_pct.y() + t * aby;

        const double dx = projx - hp.x();
        const double dy = projy - hp.y();
        double dist = std::sqrt(dx * dx + dy * dy);

        double kspace = 1.0 - dist / falloff_pct;
        if (kspace < 0.0) {
            kspace = 0.0;
        }

        kspace = kspace * kspace;

        double impact = kspace * hp_info.strength;
        if (impact > best_impact) {
            best_impact = impact;
        }
    }

    const double k = ktime * best_impact;
    return k;
}

void stream_cell::draw_wave_overlay(
    QPainter& p, const line_instance& line_value
) const {
    const QString key = line_value.template_name.trimmed();
    if (key.isEmpty() || !line_waves.contains(key)) {
        return;
    }

    const QVector<line_wave_pulse> pulses = line_waves.value(key);
    if (pulses.isEmpty()) {
        return;
    }

    const auto geometry = stream_cell_support::build_path_geometry(line_value);
    if (geometry.segments.empty()) {
        return;
    }

    const auto wave_style
        = stream_cell_support::line_wave_style_for_line(line_value);
    const qint64 now_ms = animation_clock.elapsed();
    QPolygonF wave_poly;
    bool has_visible_wave = false;

    for (size_t segment_i = 0; segment_i < geometry.segments.size(); ++segment_i) {
        const auto& segment = geometry.segments[segment_i];
        const QLineF segment_px(to_px(segment.a_pct), to_px(segment.b_pct));
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
            const double t = static_cast<double>(sample_i)
                / static_cast<double>(samples);
            const QPointF base_pct(
                segment.a_pct.x() + (segment.b_pct.x() - segment.a_pct.x()) * t,
                segment.a_pct.y() + (segment.b_pct.y() - segment.a_pct.y()) * t
            );
            const QPointF base_px = to_px(base_pct);
            const double sample_path_pos = segment.start_pos + t * segment.length;
            const double displacement_px
                = stream_cell_support::line_wave_displacement_px(
                    geometry, segment.index, sample_path_pos, now_ms, pulses
                );

            if (std::abs(displacement_px) > 0.05) {
                has_visible_wave = true;
            }

            wave_poly << QPointF(
                base_px.x() + normal_px.x() * displacement_px,
                base_px.y() + normal_px.y() * displacement_px
            );
        }
    }

    if (!has_visible_wave || wave_poly.size() < 2) {
        return;
    }

    QColor glow = line_value.color.lighter(160);
    glow.setAlpha(
        std::clamp(
            static_cast<int>(110.0 * wave_style.glow_width_k), 80, 180
        )
    );
    QPen glow_pen(glow);
    glow_pen.setWidthF(
        2.8 + line_width_visual_value(line_value.width_text)
            * 0.42 * wave_style.glow_width_k
    );
    p.setPen(glow_pen);
    p.drawPolyline(wave_poly);

    QColor wave_color = line_value.color.lighter(125);
    wave_color.setAlpha(
        std::clamp(
            static_cast<int>(208.0 + (wave_style.amplitude_k - 1.0) * 36.0),
            160, 245
        )
    );
    QPen wave_pen(wave_color);
    wave_pen.setWidthF(
        std::max(
            1.8,
            line_width_visual_value(line_value.width_text)
                * (0.52 + (wave_style.spread_k - 1.0) * 0.18)
        )
    );
    p.setPen(wave_pen);
    p.drawPolyline(wave_poly);
}

void stream_cell::add_line_wave(
    const QString& line_name, const QPointF& pos_pct, double strength,
    const QString& direction, double speed
) {
    const QString key = line_name.trimmed();
    if (key.isEmpty()) {
        return;
    }

    const line_instance* line_value = nullptr;
    for (const auto& candidate : persistent_lines) {
        if (candidate.template_name.trimmed() == key) {
            line_value = &candidate;
            break;
        }
    }

    if (line_value == nullptr) {
        return;
    }

    const auto geometry = stream_cell_support::build_path_geometry(*line_value);
    if (geometry.segments.empty()) {
        return;
    }

    const auto projection = stream_cell_support::project_hit(geometry, pos_pct);
    if (!projection.has_value()) {
        return;
    }

    const qint64 now_ms = animation_clock.elapsed();
    const double clamped_strength = std::clamp(strength, 0.2, 1.4);
    const double clamped_speed = std::clamp(speed, 0.25, 2.5);
    const int displacement_sign
        = direction == QStringLiteral("pos_to_neg") ? -1 : 1;
    const auto wave_style
        = stream_cell_support::line_wave_style_for_line(*line_value);

    auto& pulses = line_waves[key];

    for (int travel_sign = -1; travel_sign <= 1; travel_sign += 2) {
        line_wave_pulse pulse;
        pulse.source_segment_index = projection->segment_index;
        pulse.start_ms = now_ms;
        pulse.origin_path_pos = projection->path_pos;
        pulse.amplitude = std::clamp(
            (5.0 + 9.0 * clamped_strength * clamped_speed)
                * wave_style.amplitude_k,
            3.0, 24.0
        );
        pulse.speed = std::clamp(
            (18.0 + 38.0 * clamped_speed) * wave_style.speed_k, 10.0, 80.0
        );
        pulse.spread = std::clamp(
            (7.0 + 10.0 * clamped_strength) * wave_style.spread_k, 4.0, 28.0
        );
        pulse.frequency_hz = std::clamp(
            (2.2 + 1.6 * clamped_speed) * wave_style.frequency_k, 1.0, 7.5
        );
        pulse.damping_per_s = std::clamp(
            (1.1 + 0.8 * (1.2 - std::min(clamped_strength, 1.2)))
                * wave_style.damping_k,
            0.35, 2.2
        );
        pulse.travel_sign = travel_sign;
        pulse.displacement_sign = displacement_sign;

        bool merged = false;
        for (int i = 0; i < pulses.size(); i += 1) {
            auto& existing = pulses[i];
            const qint64 merge_age = now_ms - existing.start_ms;
            if (existing.source_segment_index != pulse.source_segment_index
                || existing.travel_sign != pulse.travel_sign
                || merge_age < 0
                || merge_age > stream_cell_support::wave_merge_window_ms) {
                continue;
            }

            if (pulse.amplitude > existing.amplitude) {
                existing.origin_path_pos = pulse.origin_path_pos;
                existing.amplitude = pulse.amplitude;
                existing.speed = pulse.speed;
                existing.spread = pulse.spread;
                existing.frequency_hz = pulse.frequency_hz;
                existing.damping_per_s = pulse.damping_per_s;
                existing.displacement_sign = pulse.displacement_sign;
            } else {
                existing.amplitude = std::max(existing.amplitude, pulse.amplitude);
            }

            if (pulse.start_ms < existing.start_ms) {
                existing.start_ms = pulse.start_ms;
            }

            merged = true;
            break;
        }

        if (!merged) {
            pulses.push_back(pulse);
        }
    }

    prune_line_waves(now_ms);
    update_animation_timer();
}

void stream_cell::prune_line_waves(const qint64 now_ms) {
    for (auto it = line_waves.begin(); it != line_waves.end();) {
        auto& pulses = it.value();
        for (int i = 0; i < pulses.size(); i += 1) {
            const qint64 age_ms = now_ms - pulses[i].start_ms;
            if (age_ms < stream_cell_support::wave_pulse_ttl_ms) {
                continue;
            }

            pulses.removeAt(i);
            i -= 1;
        }

        if (pulses.isEmpty()) {
            it = line_waves.erase(it);
        } else {
            ++it;
        }
    }
}

void stream_cell::update_animation_timer() {
    if (animation_timer == nullptr) {
        return;
    }

    if (line_waves.isEmpty()) {
        if (animation_timer->isActive()) {
            animation_timer->stop();
        }
        return;
    }

    if (!animation_timer->isActive()) {
        animation_timer->start();
    }
}

void stream_cell::on_close_clicked() { emit request_close(name); }

void stream_cell::on_focus_clicked() { emit request_focus(name); }

void stream_cell::on_frame_changed(const QVideoFrame& frame) {
    if (!frame.isValid()) {
        return;
    }

    QVideoFrame copy(frame);
    if (!copy.map(QVideoFrame::ReadOnly)) {
        return;
    }

    last_frame = copy.toImage();
    copy.unmap();

    emit frame_ready(name, last_frame);

    if (!repaint_timer.isValid()) {
        repaint_timer.start();
        update();
        return;
    }

    if (repaint_timer.elapsed() < repaint_interval_ms) {
        return;
    }

    repaint_timer.restart();
    update();
}

void stream_cell::on_media_status_changed(
    const QMediaPlayer::MediaStatus status
) {
    if (!loop_enabled) {
        return;
    }
    if (status != QMediaPlayer::EndOfMedia) {
        return;
    }
    if (!player) {
        return;
    }

    player->setPosition(0);
    player->play();
}

void stream_cell::on_player_error(
    const QMediaPlayer::Error error, const QString& error_string
) {
    Q_UNUSED(error);
    last_error = error_string;
    update();
}

void stream_cell::on_camera_error(const QCamera::Error error) {
    Q_UNUSED(error);

    if (!camera) {
        return;
    }

    last_error = camera->errorString();
    update();
}

void stream_cell::on_animation_tick() {
    const qint64 now_ms = animation_clock.elapsed();
    prune_line_waves(now_ms);
    update_animation_timer();

    if (!line_waves.isEmpty()) {
        update();
    }
}
