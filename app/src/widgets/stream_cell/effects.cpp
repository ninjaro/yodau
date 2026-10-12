#include "widgets/stream_cell.hpp"

#include "widgets/processing_overlay_renderer.hpp"

#include <QPainter>
#include <algorithm>
#include <cmath>
#include <utility>

void stream_cell::add_event(const QPointF& pos_pct, const QColor& color) {
    const bool had_events = !events.isEmpty();
    event_instance e;
    e.pos_pct = pos_pct;
    e.color = color;
    e.ts = QDateTime::currentDateTime();

    events.push_back(e);
    if (!had_events) {
        update_accessible_description();
    }
    update();
}

void stream_cell::set_processing_overlays(
    std::vector<processing_overlay_instance> overlays
) {
    processing_overlays = std::move(overlays);
    update_accessible_description();
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

void stream_cell::draw_processing_overlays(QPainter& p) const {
    std::vector<processing_overlay_instance> display_overlays
        = processing_overlays;
    for (processing_overlay_instance& overlay : display_overlays) {
        overlay.points_pct = display_points_pct(overlay.points_pct);
        if (overlay.anchor_pct.has_value()) {
            const std::vector<QPointF> transformed
                = display_points_pct({ *overlay.anchor_pct });
            overlay.anchor_pct = transformed.front();
        }
    }
    processing_overlay_renderer::draw(
        p, display_frame_rect(), stream_settings_value, display_overlays
    );
}

void stream_cell::draw_events(QPainter& p) {
    const QString selected_mode
        = processing_overlay_renderer::selected_movement_mode(
            stream_settings_value
        );
    if (!processing_overlay_renderer::event_bubbles_visible(
            stream_settings_value, processing_overlays
        )) {
        const bool had_events = !events.isEmpty();
        events.clear();
        if (had_events) {
            update_accessible_description();
        }
        return;
    }

    const auto now = QDateTime::currentDateTime();
    const int ttl_ms = 2000;

    const QRect r = display_frame_rect().toAlignedRect();
    const auto w = static_cast<double>(r.width());
    const auto h = static_cast<double>(r.height());
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

        const QPointF center = canonical_to_display(e.pos_pct);

        const bool draw_symbolic_algorithm_overlay
            = selected_mode == QStringLiteral("auto")
            && processing_overlays.empty()
            && stream_settings_value.algorithm_overlay_enabled;

        if (draw_symbolic_algorithm_overlay) {
            processing_overlay_renderer::draw_event_bubble(
                p, r, center, radius, c, stream_settings_value, k
            );
        }

        p.setPen(Qt::NoPen);
        p.setBrush(color_with_alpha(
            c, draw_symbolic_algorithm_overlay ? static_cast<int>(210.0 * k) : a
        ));
        p.drawEllipse(
            center, draw_symbolic_algorithm_overlay ? radius * 0.55 : radius,
            draw_symbolic_algorithm_overlay ? radius * 0.55 : radius
        );
    }

    const bool all_events_expired = !events.isEmpty() && alive.isEmpty();
    events = std::move(alive);
    if (all_events_expired) {
        update_accessible_description();
    }
}

double stream_cell::segment_impact_k(
    const QPointF& a_pct, const QPointF& b_pct,
    const QVector<hit_info>& hit_points, double falloff_pct, double ktime
) {
    const double abx = b_pct.x() - a_pct.x();
    const double aby = b_pct.y() - a_pct.y();
    const double ab_len2 = abx * abx + aby * aby;

    if (ab_len2 <= 0.0) {
        return 0.0;
    }

    double best_impact = 0.0;

    for (int j = 0; j < hit_points.size(); j += 1) {
        const hit_info& hp_info = hit_points[j];
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
    QPainter& p, const line_instance& line_value, const QColor& line_color
) const {
    const QString key = line_value.template_name.trimmed();
    if (key.isEmpty() || !line_waves.contains(key)) {
        return;
    }

    const auto pulses = line_waves.value(key);
    if (pulses.isEmpty()) {
        return;
    }

    const QRectF frame_rect = display_frame_rect();
    const QSize target_size = frame_rect.size().toSize();
    const auto display_geometry = line_wave_renderer::build_path_geometry(
        display_points_pct(line_value.pts_pct), line_value.closed
    );
    p.save();
    p.translate(frame_rect.topLeft());
    line_wave_renderer::draw(
        p, display_geometry, pulses, target_size, line_color,
        line_value.width_text, line_value.length_text, line_value.response_text,
        animation_clock.elapsed()
    );
    p.restore();
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
        if (candidate.enabled && candidate.template_name.trimmed() == key) {
            line_value = &candidate;
            break;
        }
    }

    if (line_value == nullptr) {
        return;
    }

    const auto geometry = line_wave_renderer::build_path_geometry(
        line_value->pts_pct, line_value->closed
    );
    if (geometry.segments.empty()) {
        return;
    }

    const auto projection = line_wave_renderer::project_hit(geometry, pos_pct);
    if (!projection.has_value()) {
        return;
    }

    const qint64 now_ms = animation_clock.elapsed();
    line_wave_renderer::add_pulses(
        line_waves[key], *projection, now_ms, strength, direction, speed,
        line_value->width_text, line_value->length_text,
        line_value->response_text
    );

    prune_line_waves(now_ms);
    update_animation_timer();
}

void stream_cell::prune_line_waves(const qint64 now_ms) {
    line_wave_renderer::prune(line_waves, now_ms);
}

void stream_cell::update_animation_timer() {
    if (animation_timer == nullptr) {
        return;
    }

    const bool edit_pulse_active
        = line_edit_preview_.has_value() && line_edit_source_line_.has_value();
    if (line_waves.isEmpty() && !edit_pulse_active) {
        if (animation_timer->isActive()) {
            animation_timer->stop();
        }
        return;
    }

    if (!animation_timer->isActive()) {
        animation_timer->start();
    }
}

void stream_cell::on_animation_tick() {
    const qint64 now_ms = animation_clock.elapsed();
    prune_line_waves(now_ms);
    update_animation_timer();

    if (!line_waves.isEmpty()
        || (line_edit_preview_.has_value()
            && line_edit_source_line_.has_value())) {
        update();
    }
}
