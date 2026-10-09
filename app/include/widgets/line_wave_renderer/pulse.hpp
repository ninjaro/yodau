#ifndef YODAU_APP_WIDGETS_LINE_WAVE_RENDERER_PULSE_HPP
#define YODAU_APP_WIDGETS_LINE_WAVE_RENDERER_PULSE_HPP

#include "shell/app_settings.hpp"
#include "widgets/line_wave_renderer/path.hpp"

#include <QHash>
#include <QString>
#include <QVector>

#include <numbers>

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
        static_cast<double>(line_width_visual_value(width_text)) / 3.0, 0.6, 2.2
    );
    const double length_scale = line_length_scale(length_text);
    const double response_scale = line_response_scale(response_text);

    visual_style style;
    style.amplitude_k = std::clamp(
        0.74 + width_scale * 0.24 + (response_scale - 1.0) * 0.28, 0.55, 1.85
    );
    style.speed_k = std::clamp(
        1.08 - (width_scale - 1.0) * 0.18 - (length_scale - 1.0) * 0.40, 0.55,
        1.45
    );
    style.spread_k = std::clamp(
        0.84 + width_scale * 0.18 + length_scale * 0.20
            + (response_scale - 1.0) * 0.18,
        0.65, 1.95
    );
    style.frequency_k = std::clamp(
        1.16 - (width_scale - 1.0) * 0.16 - (length_scale - 1.0) * 0.42, 0.5,
        1.65
    );
    style.damping_k = std::clamp(
        1.08 - (width_scale - 1.0) * 0.08 - (length_scale - 1.0) * 0.16
            - (response_scale - 1.0) * 0.48,
        0.45, 1.55
    );
    style.glow_width_k = std::clamp(
        0.9 + width_scale * 0.16 + (response_scale - 1.0) * 0.14, 0.75, 1.55
    );
    return style;
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
        const double joint_factor
            = std::pow(joint_transfer, static_cast<double>(hops));
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
            (5.0 + 9.0 * clamped_strength * clamped_speed) * style.amplitude_k,
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
                || merge_age < 0 || merge_age > merge_window_ms) {
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
                existing.amplitude
                    = std::max(existing.amplitude, next_pulse.amplitude);
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

#endif
