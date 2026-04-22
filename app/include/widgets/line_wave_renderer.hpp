#ifndef YODAU_APP_WIDGETS_LINE_WAVE_RENDERER_HPP
#define YODAU_APP_WIDGETS_LINE_WAVE_RENDERER_HPP

#include "shell/app_settings.hpp"

#include <QColor>
#include <QHash>
#include <QLineF>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QPolygonF>
#include <QSize>
#include <QString>
#include <QVector>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <vector>

namespace line_wave_renderer {

constexpr int animation_interval_ms = 16;
constexpr qint64 pulse_ttl_ms = 2400;
constexpr qint64 merge_window_ms = 120;
constexpr double joint_transfer = 0.72;

struct pulse {
    int source_segment_index { -1 };
    qint64 start_ms { 0 };
    double origin_path_pos { 0.0 };
    double amplitude { 0.0 };
    double speed { 0.0 };
    double spread { 0.0 };
    double frequency_hz { 0.0 };
    double damping_per_s { 0.0 };
    int travel_sign { 1 };
    int displacement_sign { 1 };
};

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

struct visual_style {
    double amplitude_k { 1.0 };
    double speed_k { 1.0 };
    double spread_k { 1.0 };
    double frequency_k { 1.0 };
    double damping_k { 1.0 };
    double glow_width_k { 1.0 };
};

inline double clamp_unit(const double value) {
    return std::clamp(value, 0.0, 1.0);
}

inline double point_distance_pct(const QPointF& a_pct, const QPointF& b_pct) {
    const double dx = b_pct.x() - a_pct.x();
    const double dy = b_pct.y() - a_pct.y();
    return std::sqrt(dx * dx + dy * dy);
}

inline double normalized_numeric_value(const QString& text) {
    bool ok = false;
    const double value = text.toDouble(&ok);
    if (!ok || value <= 0.0) {
        return -1.0;
    }

    return value;
}

inline double line_length_scale(const QString& length_text) {
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

inline double line_response_scale(const QString& response_text) {
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

inline visual_style style_for_line(
    const QString& width_text, const QString& length_text,
    const QString& response_text
) {
    const double width_scale = std::clamp(
        static_cast<double>(line_width_visual_value(width_text)) / 3.0,
        0.6, 2.2
    );
    const double length_scale = line_length_scale(length_text);
    const double response_scale = line_response_scale(response_text);

    visual_style style;
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

inline path_geometry build_path_geometry(
    const std::vector<QPointF>& pts_pct, const bool closed
) {
    path_geometry geometry;
    geometry.closed = closed;

    if (pts_pct.size() < 2) {
        return geometry;
    }

    geometry.segments.reserve(pts_pct.size() + (closed ? 1u : 0u));

    for (size_t i = 1; i < pts_pct.size(); ++i) {
        const QPointF a_pct = pts_pct[i - 1];
        const QPointF b_pct = pts_pct[i];
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

    if (closed && pts_pct.size() > 2) {
        const QPointF a_pct = pts_pct.back();
        const QPointF b_pct = pts_pct.front();
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

inline std::optional<hit_projection>
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
        const double t = std::clamp((apx * abx + apy * aby) / len2, 0.0, 1.0);

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

inline int segment_hops(
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

inline double forward_distance(
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

inline double displacement_px(
    const path_geometry& geometry, const int segment_index,
    const double sample_path_pos, const qint64 now_ms,
    const QVector<pulse>& pulses
) {
    double displacement = 0.0;

    for (const auto& pulse_value : pulses) {
        const qint64 age_ms = now_ms - pulse_value.start_ms;
        if (age_ms < 0 || age_ms >= pulse_ttl_ms) {
            continue;
        }

        const double age_s = static_cast<double>(age_ms) / 1000.0;
        const double travel = pulse_value.speed * age_s;
        const double sample_progress = forward_distance(
            geometry, pulse_value.origin_path_pos, sample_path_pos,
            pulse_value.travel_sign
        );

        if (!geometry.closed && sample_progress < 0.0) {
            continue;
        }

        const double spread = pulse_value.spread * (1.0 + age_s * 0.2);
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
            geometry, pulse_value.source_segment_index, segment_index
        );
        const double joint_factor = std::pow(
            joint_transfer, static_cast<double>(hops)
        );
        const double time_decay = std::exp(-pulse_value.damping_per_s * age_s);

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
            = 2.0 * std::numbers::pi * pulse_value.frequency_hz * age_s
            - sample_progress * 0.35;

        displacement += pulse_value.displacement_sign * pulse_value.amplitude
            * joint_factor * time_decay * envelope * edge_factor
            * std::sin(phase);
    }

    return displacement;
}

inline QPointF to_px(const QPointF& pos_pct, const QSize& target_size) {
    return {
        pos_pct.x() / 100.0 * static_cast<double>(target_size.width()),
        pos_pct.y() / 100.0 * static_cast<double>(target_size.height())
    };
}

inline bool draw(
    QPainter& painter, const path_geometry& geometry, const QVector<pulse>& pulses,
    const QSize& target_size, const QColor& line_color,
    const QString& width_text, const QString& length_text,
    const QString& response_text, const qint64 now_ms
) {
    if (pulses.isEmpty() || geometry.segments.empty()) {
        return false;
    }

    const auto style = style_for_line(width_text, length_text, response_text);
    QPolygonF wave_poly;
    bool has_visible_wave = false;

    for (size_t segment_i = 0; segment_i < geometry.segments.size(); ++segment_i) {
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
            const double t = static_cast<double>(sample_i)
                / static_cast<double>(samples);
            const QPointF base_pct(
                segment.a_pct.x() + (segment.b_pct.x() - segment.a_pct.x()) * t,
                segment.a_pct.y() + (segment.b_pct.y() - segment.a_pct.y()) * t
            );
            const QPointF base_px = to_px(base_pct, target_size);
            const double sample_path_pos = segment.start_pos + t * segment.length;
            const double sample_displacement_px = displacement_px(
                geometry, segment.index, sample_path_pos, now_ms, pulses
            );

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
            static_cast<int>(208.0 + (style.amplitude_k - 1.0) * 36.0),
            160, 245
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

inline void add_pulses(
    QVector<pulse>& pulses, const hit_projection& projection,
    const qint64 now_ms, const double strength, const QString& direction,
    const double speed, const QString& width_text, const QString& length_text,
    const QString& response_text
) {
    const double clamped_strength = std::clamp(strength, 0.2, 1.4);
    const double clamped_speed = std::clamp(speed, 0.25, 2.5);
    const int displacement_sign
        = direction == QStringLiteral("pos_to_neg") ? -1 : 1;
    const auto style = style_for_line(width_text, length_text, response_text);

    for (int travel_sign = -1; travel_sign <= 1; travel_sign += 2) {
        pulse next_pulse;
        next_pulse.source_segment_index = projection.segment_index;
        next_pulse.start_ms = now_ms;
        next_pulse.origin_path_pos = projection.path_pos;
        next_pulse.amplitude = std::clamp(
            (5.0 + 9.0 * clamped_strength * clamped_speed)
                * style.amplitude_k,
            3.0, 24.0
        );
        next_pulse.speed = std::clamp(
            (18.0 + 38.0 * clamped_speed) * style.speed_k, 10.0, 80.0
        );
        next_pulse.spread = std::clamp(
            (7.0 + 10.0 * clamped_strength) * style.spread_k, 4.0, 28.0
        );
        next_pulse.frequency_hz = std::clamp(
            (2.2 + 1.6 * clamped_speed) * style.frequency_k, 1.0, 7.5
        );
        next_pulse.damping_per_s = std::clamp(
            (1.1 + 0.8 * (1.2 - std::min(clamped_strength, 1.2)))
                * style.damping_k,
            0.35, 2.2
        );
        next_pulse.travel_sign = travel_sign;
        next_pulse.displacement_sign = displacement_sign;

        bool merged = false;
        for (int i = 0; i < pulses.size(); i += 1) {
            auto& existing = pulses[i];
            const qint64 merge_age = now_ms - existing.start_ms;
            if (existing.source_segment_index != next_pulse.source_segment_index
                || existing.travel_sign != next_pulse.travel_sign
                || merge_age < 0
                || merge_age > merge_window_ms) {
                continue;
            }

            if (next_pulse.amplitude > existing.amplitude) {
                existing.origin_path_pos = next_pulse.origin_path_pos;
                existing.amplitude = next_pulse.amplitude;
                existing.speed = next_pulse.speed;
                existing.spread = next_pulse.spread;
                existing.frequency_hz = next_pulse.frequency_hz;
                existing.damping_per_s = next_pulse.damping_per_s;
                existing.displacement_sign = next_pulse.displacement_sign;
            } else {
                existing.amplitude = std::max(
                    existing.amplitude, next_pulse.amplitude
                );
            }

            if (next_pulse.start_ms < existing.start_ms) {
                existing.start_ms = next_pulse.start_ms;
            }

            merged = true;
            break;
        }

        if (!merged) {
            pulses.push_back(next_pulse);
        }
    }
}

inline void prune(QHash<QString, QVector<pulse>>& waves, const qint64 now_ms) {
    for (auto it = waves.begin(); it != waves.end();) {
        auto& pulses = it.value();
        for (int i = 0; i < pulses.size(); i += 1) {
            const qint64 age_ms = now_ms - pulses[i].start_ms;
            if (age_ms < pulse_ttl_ms) {
                continue;
            }

            pulses.removeAt(i);
            i -= 1;
        }

        if (pulses.isEmpty()) {
            it = waves.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace line_wave_renderer

#endif // YODAU_APP_WIDGETS_LINE_WAVE_RENDERER_HPP
