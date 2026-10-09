#include "widgets/stream_cell.hpp"

#include <QLabel>
#include <QPushButton>
#include <algorithm>
#include <cmath>

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
    stream_settings_value.algorithm_id = default_app_algorithm_id();
    stream_settings_value.algorithm_preset
        = default_algorithm_preset_id(stream_settings_value.algorithm_id);
    build_ui();
    build_resize_handle();
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(tr("Video stream %1").arg(name));
    update_accessible_description();
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
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

bool stream_cell::is_drawing_enabled() const { return drawing_enabled; }

bool stream_cell::has_line_edit_preview() const {
    return line_edit_preview_.has_value();
}

bool stream_cell::application_active() const { return application_active_; }

QString stream_cell::line_edit_preview_name() const {
    return line_edit_preview_.has_value() ? line_edit_preview_->template_name
                                          : QString();
}

double stream_cell::source_frame_aspect() const noexcept {
    return source_frame_aspect_;
}

double stream_cell::effective_display_aspect() const noexcept {
    return quarter_turns_ % 2 == 0 ? source_frame_aspect_
                                   : 1.0 / source_frame_aspect_;
}

int stream_cell::quarter_turns() const noexcept { return quarter_turns_; }

bool stream_cell::keeps_frame_aspect_ratio() const noexcept {
    return keep_frame_aspect_ratio_;
}

bool stream_cell::has_manual_short_side() const noexcept {
    return manual_short_side_.has_value();
}

double stream_cell::manual_short_side() const noexcept {
    return manual_short_side_.value_or(manual_minimum);
}

bool stream_cell::is_draft_preview() const { return draft_preview; }

stream_settings stream_cell::current_stream_settings() const {
    return stream_settings_value;
}

stream_runtime_metrics stream_cell::current_runtime_metrics() const {
    return runtime_metrics_value;
}

app_log_mode stream_cell::current_log_mode() const { return log_mode_value; }

void stream_cell::set_active(const bool val) {
    if (active == val) {
        return;
    }
    end_resize(false);
    active = val;
    if (!active) {
        set_drawing_enabled(false);
        clear_draft();
    }
    update_icon();
    update_display_controls();
    update_resize_handle();
    update_accessible_description();
    update();
}

void stream_cell::set_quarter_turns(const int turns) {
    const int normalized = ((turns % 4) + 4) % 4;
    if (quarter_turns_ == normalized) {
        return;
    }
    end_resize(false);
    quarter_turns_ = normalized;
    update_display_controls();
    update_accessible_description();
    update();
    emit layout_geometry_changed();
}

void stream_cell::set_keep_frame_aspect_ratio(const bool keep) {
    if (keep_frame_aspect_ratio_ == keep) {
        return;
    }
    keep_frame_aspect_ratio_ = keep;
    update_display_controls();
    update();
}

void stream_cell::set_fixed_size_controls_visible(const bool visible) {
    if (fixed_size_controls_visible_ == visible) {
        return;
    }
    end_resize(false);
    fixed_size_controls_visible_ = visible;
    update_display_controls();
    update_resize_handle();
}

void stream_cell::initialize_manual_short_side(const double short_side) {
    if (!std::isfinite(short_side)) {
        return;
    }
    manual_short_side_ = std::max(manual_minimum, short_side);
    update_display_controls();
}

void stream_cell::set_drawing_enabled(const bool on) {
    drawing_enabled = on;
    if (on && !hover_point_pct.has_value()) {
        hover_point_pct = QPointF(50.0, 50.0);
    } else if (!on) {
        hover_point_pct.reset();
    }
    sync_mouse_tracking();
    update_accessible_description();
    update();
}

void stream_cell::set_stream_settings(const stream_settings& settings_value) {
    stream_settings_value = settings_value;
    if (stream_settings_value.stream_name.trimmed().isEmpty()) {
        stream_settings_value.stream_name = name;
    }
    stream_settings_value.algorithm_id
        = normalized_app_algorithm_id(stream_settings_value.algorithm_id);
    stream_settings_value.algorithm_preset = normalized_algorithm_preset_id(
        stream_settings_value.algorithm_id,
        stream_settings_value.algorithm_preset
    );
    stream_settings_value.movement_display_mode
        = normalized_movement_display_mode_id(
            stream_settings_value.movement_display_mode
        );
    stream_settings_value.algorithm_overlay_enabled
        = movement_display_enabled(stream_settings_value.movement_display_mode);
    update();
}

void stream_cell::set_runtime_metrics(const stream_runtime_metrics& metrics) {
    runtime_metrics_value = metrics;
    update();
}

void stream_cell::set_log_mode(const app_log_mode mode) {
    if (log_mode_value == mode) {
        return;
    }

    log_mode_value = mode;
    update();
}

void stream_cell::set_draft_params(
    const QString& n, const QColor& color, const bool closed,
    const QString& color_mode_id, const QString& width_text,
    const QString& length_text, const QString& response_text
) {
    draft_line_name = n;
    draft_line_color = color;
    draft_line_color_mode_id = normalized_line_color_mode_id(color_mode_id);
    draft_line_closed = closed;
    draft_line_width_text = normalized_line_width_text(width_text);
    draft_line_length_text = normalized_line_length_text(length_text);
    draft_line_response_text = normalized_line_response_text(response_text);
    update_accessible_description();
    update();
}

void stream_cell::set_draft_points_pct(const std::vector<QPointF>& pts) {
    draft_line_points_pct = pts;
    update_accessible_description();
    update();
}

void stream_cell::clear_draft() {
    draft_line_points_pct.clear();
    hover_point_pct.reset();
    draft_preview = false;
    draft_line_color_mode_id = default_line_color_mode_id();
    draft_line_width_text = default_line_width_text();
    draft_line_length_text = default_line_length_text();
    draft_line_response_text = default_line_response_text();
    sync_mouse_tracking();
    update_accessible_description();
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
    update_accessible_description();
    update();
}

void stream_cell::add_persistent_line(const line_instance& line) {
    persistent_lines.push_back(line);
    update_accessible_description();
    update();
}

void stream_cell::clear_persistent_lines() {
    persistent_lines.clear();
    line_highlights.clear();
    line_hits.clear();
    line_waves.clear();
    update_animation_timer();
    update_accessible_description();
    update();
}

void stream_cell::set_draft_preview(const bool on) {
    draft_preview = on;
    update();
}

void stream_cell::set_line_edit_preview(
    const std::optional<line_instance>& line_value
) {
    const bool had_preview = line_edit_preview_.has_value();
    line_edit_preview_ = line_value;
    if (!line_edit_preview_.has_value()) {
        line_edit_interaction_.clear_preview();
    } else {
        line_edit_interaction_.sync_preview_selection(
            line_edit_preview_->selected_visible_index,
            static_cast<int>(line_edit_preview_->pts_pct.size())
        );
        if (!had_preview) {
            reset_line_edit_drag_state();
        }
    }
    sync_mouse_tracking();
    update_animation_timer();
    update_accessible_description();
    update();
}

void stream_cell::set_line_edit_source_line(
    const std::optional<line_instance>& line_value
) {
    line_edit_source_line_ = line_value;
    update_animation_timer();
    update_accessible_description();
    update();
}

void stream_cell::set_labels_enabled(const bool on) {
    if (labels_enabled == on) {
        return;
    }
    labels_enabled = on;
    update();
}
