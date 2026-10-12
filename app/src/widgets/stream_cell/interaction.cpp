#include "widgets/stream_cell.hpp"

#include <QLineF>
#include <QMouseEvent>
#include <QPushButton>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>

namespace stream_cell_support {

#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
constexpr qreal edit_vertex_hit_radius_px = 24.0;
constexpr qreal edit_segment_hit_radius_px = 18.0;
constexpr int line_edit_drag_threshold_px = 8;
#else
constexpr qreal edit_vertex_hit_radius_px = 11.0;
constexpr qreal edit_segment_hit_radius_px = 8.0;
constexpr int line_edit_drag_threshold_px = 4;
#endif
constexpr double line_edit_key_nudge_px = 1.0;
constexpr double edit_key_nudge_fast_px = 5.0;
constexpr double edit_wheel_degrees_per_step = 5.0;
constexpr double edit_wheel_fine_step_degrees = 1.0;

double point_segment_distance_px(
    const QPointF& point_px, const QPointF& a_px, const QPointF& b_px
) {
    const QLineF segment(a_px, b_px);
    const double segment_length_sq
        = segment.dx() * segment.dx() + segment.dy() * segment.dy();
    if (segment_length_sq <= std::numeric_limits<double>::epsilon()) {
        return QLineF(point_px, a_px).length();
    }

    const QPointF ap = point_px - a_px;
    const QPointF ab = b_px - a_px;
    const double projection = std::clamp(
        (ap.x() * ab.x() + ap.y() * ab.y()) / segment_length_sq, 0.0, 1.0
    );
    const QPointF closest = a_px + ab * projection;
    return QLineF(point_px, closest).length();
}

QPointF nearest_segment_point(
    const QPointF& point_px, const QPointF& a_px, const QPointF& b_px
) {
    const QPointF ab = b_px - a_px;
    const double segment_length_sq = ab.x() * ab.x() + ab.y() * ab.y();
    if (segment_length_sq <= std::numeric_limits<double>::epsilon()) {
        return a_px;
    }

    const QPointF ap = point_px - a_px;
    const double projection = std::clamp(
        (ap.x() * ab.x() + ap.y() * ab.y()) / segment_length_sq, 0.0, 1.0
    );
    return a_px + ab * projection;
}

} // namespace stream_cell_support

void stream_cell::mousePressEvent(QMouseEvent* event) {
    auto* child = childAt(event->pos());
    if (child == close_btn || child == focus_btn || child == rotate_btn
        || child == aspect_mode_btn || child == manual_size_decrease_btn
        || child == manual_size_increase_btn) {
        QWidget::mousePressEvent(event);
        return;
    }

    if (active && line_edit_preview_.has_value()
        && event->button() == Qt::LeftButton) {
        setFocus();
        hover_point_pct = to_pct(event->pos());
        const auto pressed_vertex = line_edit_vertex_at(event->pos());
        const auto segment_hit = line_edit_segment_hit(event->pos());
        const auto press_result = line_edit_interaction_.begin_press(
            pressed_vertex, segment_hit.has_value(), to_pct(event->pos()),
            event->pos()
        );

        if (press_result.selected_vertex.has_value()) {
            emit line_edit_point_selected(*press_result.selected_vertex);
        }

        update_line_edit_cursor(event->pos());
        update();
        event->accept();
        return;
    }

    if (!drawing_enabled || !active) {
        QWidget::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        setFocus();
        draft_line_points_pct.push_back(to_pct(event->pos()));
        update_accessible_description();
        update();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void stream_cell::mouseReleaseEvent(QMouseEvent* event) {
    if (active && line_edit_preview_.has_value()
        && event->button() == Qt::LeftButton) {
        hover_point_pct = to_pct(event->pos());

        const bool was_dragging = line_edit_interaction_.release_was_dragging();
        if (line_edit_interaction_.press_active()
            && !line_edit_interaction_.press_moved()) {
            update();
        }

        if (was_dragging) {
            emit line_edit_change_finished();
        }
        reset_line_edit_drag_state();
        update_line_edit_cursor(event->pos());
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void stream_cell::mouseDoubleClickEvent(QMouseEvent* event) {
    auto* child = childAt(event->pos());
    if (child == close_btn || child == focus_btn || child == rotate_btn
        || child == aspect_mode_btn || child == manual_size_decrease_btn
        || child == manual_size_increase_btn) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }

    if (active && line_edit_preview_.has_value()
        && event->button() == Qt::LeftButton) {
        if (const auto vertex_index = line_edit_vertex_at(event->pos());
            vertex_index.has_value()) {
            line_edit_interaction_.set_selected_vertex(vertex_index);
            emit line_edit_point_selected(*vertex_index);
            emit line_edit_point_split_requested(*vertex_index);
            update();
            event->accept();
            return;
        }
        if (const auto segment_hit = line_edit_segment_hit(event->pos());
            segment_hit.has_value()) {
            line_edit_interaction_.set_selected_vertex(
                segment_hit->visible_segment_index + 1
            );
            emit line_edit_segment_insert_requested(
                segment_hit->visible_segment_index, segment_hit->point_pct
            );
            update();
            event->accept();
            return;
        }
    }

    QWidget::mouseDoubleClickEvent(event);
}

void stream_cell::mouseMoveEvent(QMouseEvent* event) {
    if (active && line_edit_preview_.has_value()) {
        hover_point_pct = to_pct(event->pos());
        update_line_edit_cursor(event->pos());

        if (line_edit_interaction_.press_active()
            && (event->buttons() & Qt::LeftButton)
            && line_edit_interaction_.press_hit_shape()) {
            if (line_edit_interaction_.start_drag_if_threshold(
                    event->pos(),
                    stream_cell_support::line_edit_drag_threshold_px
                )) {
                emit line_edit_change_started();
            }

            if (line_edit_interaction_.press_moved()) {
                setCursor(Qt::ClosedHandCursor);
                if (line_edit_interaction_.current_drag_mode()
                    == line_edit_interaction::drag_mode::shape) {
                    const QPointF current_pct = clamp_pct(to_pct(event->pos()));
                    const QPointF delta_pct = current_pct
                        - line_edit_interaction_.press_origin_pct();
                    emit line_edit_shape_drag_requested(delta_pct);
                    line_edit_interaction_.update_press_origin(
                        current_pct, event->pos()
                    );
                } else if (
                    line_edit_interaction_.current_drag_mode()
                        == line_edit_interaction::drag_mode::point
                    && line_edit_interaction_.pressed_vertex().has_value()
                ) {
                    line_edit_interaction_.set_selected_vertex(
                        line_edit_interaction_.pressed_vertex()
                    );
                    emit line_edit_point_move_requested(
                        *line_edit_interaction_.pressed_vertex(),
                        clamp_pct(to_pct(event->pos()))
                    );
                }
            }
        }

        update();
        event->accept();
        return;
    }

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
    if (line_edit_preview_.has_value()) {
        setCursor(Qt::CrossCursor);
    }
    update();
    QWidget::leaveEvent(event);
}

void stream_cell::keyPressEvent(QKeyEvent* event) {
    if (resizing_ && event->key() == Qt::Key_Escape) {
        end_resize(false);
        event->accept();
        return;
    }
    if (active && line_edit_preview_.has_value()) {
        const bool control_pressed
            = (event->modifiers() & Qt::ControlModifier) != 0;
        if (control_pressed && event->key() == Qt::Key_Z) {
            setFocus();
            if ((event->modifiers() & Qt::ShiftModifier) != 0) {
                emit line_edit_redo_requested();
            } else {
                emit line_edit_undo_requested();
            }
            line_edit_interaction_.set_selected_vertex(std::nullopt);
            update();
            event->accept();
            return;
        }

        if (control_pressed && event->key() == Qt::Key_Y) {
            setFocus();
            emit line_edit_redo_requested();
            line_edit_interaction_.set_selected_vertex(std::nullopt);
            update();
            event->accept();
            return;
        }

        if (event->key() == Qt::Key_Escape) {
            setFocus();
            line_edit_interaction_.set_selected_vertex(std::nullopt);
            emit line_edit_revert_requested();
            update();
            event->accept();
            return;
        }

        if ((event->key() == Qt::Key_Delete
             || event->key() == Qt::Key_Backspace)
            && line_edit_interaction_.selected_vertex().has_value()) {
            setFocus();
            emit line_edit_point_delete_requested(
                *line_edit_interaction_.selected_vertex()
            );
            update();
            event->accept();
            return;
        }

        const double step_px = (event->modifiers() & Qt::ShiftModifier) != 0
            ? stream_cell_support::edit_key_nudge_fast_px
            : stream_cell_support::line_edit_key_nudge_px;
        QPointF display_delta_px;
        bool handled = true;

        switch (event->key()) {
        case Qt::Key_Left:
            display_delta_px = QPointF(-step_px, 0.0);
            break;
        case Qt::Key_Right:
            display_delta_px = QPointF(step_px, 0.0);
            break;
        case Qt::Key_Up:
            display_delta_px = QPointF(0.0, -step_px);
            break;
        case Qt::Key_Down:
            display_delta_px = QPointF(0.0, step_px);
            break;
        default:
            handled = false;
            break;
        }

        if (handled) {
            setFocus();
            const auto selected_vertex
                = line_edit_interaction_.selected_vertex();
            QPointF target_delta_pct;
            if (selected_vertex.has_value() && *selected_vertex >= 0
                && *selected_vertex
                    < static_cast<int>(line_edit_preview_->pts_pct.size())) {
                const QPointF current_point = line_edit_preview_->pts_pct.at(
                    static_cast<std::vector<QPointF>::size_type>(
                        *selected_vertex
                    )
                );
                const QPointF target_point = display_to_canonical(
                    canonical_to_display(current_point) + display_delta_px
                );
                target_delta_pct = target_point - current_point;
                emit line_edit_point_move_requested(
                    *selected_vertex,
                    clamp_pct(current_point + target_delta_pct)
                );
            } else {
                const QPointF display_center = display_frame_rect().center();
                target_delta_pct
                    = display_to_canonical(display_center + display_delta_px)
                    - display_to_canonical(display_center);
                emit line_edit_shape_drag_requested(target_delta_pct);
            }
            update();
            event->accept();
            return;
        }
    }

    if (!(drawing_enabled && active)) {
        QWidget::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Escape) {
        clear_draft();
        hover_point_pct = QPointF(50.0, 50.0);
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return
        || event->key() == Qt::Key_Enter) {
        if (!hover_point_pct.has_value()) {
            hover_point_pct = QPointF(50.0, 50.0);
        }
        draft_line_points_pct.push_back(*hover_point_pct);
        update_accessible_description();
        update();
        event->accept();
        return;
    }

    QPointF display_keyboard_delta;
    const double keyboard_step
        = (event->modifiers() & Qt::ShiftModifier) != 0 ? 5.0 : 1.0;
    switch (event->key()) {
    case Qt::Key_Left:
        display_keyboard_delta.setX(-keyboard_step);
        break;
    case Qt::Key_Right:
        display_keyboard_delta.setX(keyboard_step);
        break;
    case Qt::Key_Up:
        display_keyboard_delta.setY(-keyboard_step);
        break;
    case Qt::Key_Down:
        display_keyboard_delta.setY(keyboard_step);
        break;
    default:
        break;
    }
    if (!display_keyboard_delta.isNull()) {
        const QPointF current = hover_point_pct.value_or(QPointF(50.0, 50.0));
        const QRectF frame_rect = display_frame_rect();
        const QPointF display_delta_px(
            display_keyboard_delta.x() / 100.0 * frame_rect.width(),
            display_keyboard_delta.y() / 100.0 * frame_rect.height()
        );
        hover_point_pct = display_to_canonical(
            canonical_to_display(current) + display_delta_px
        );
        update();
        event->accept();
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
        update_accessible_description();
        update();
    }

    event->accept();
}

void stream_cell::wheelEvent(QWheelEvent* event) {
    if (active && line_edit_preview_.has_value()
        && !line_edit_preview_->pts_pct.empty()) {
        const QPoint angle_delta = event->angleDelta();
        if (!angle_delta.isNull()) {
            const double degrees_per_step
                = (event->modifiers() & Qt::ShiftModifier) != 0
                ? stream_cell_support::edit_wheel_fine_step_degrees
                : stream_cell_support::edit_wheel_degrees_per_step;
            const double delta_degrees = static_cast<double>(angle_delta.y())
                / 120.0 * degrees_per_step;
            if (std::abs(delta_degrees)
                > std::numeric_limits<double>::epsilon()) {
                setFocus();
                emit line_edit_shape_rotate_requested(
                    delta_degrees,
                    line_edit_interaction_.selected_vertex().value_or(-1)
                );
                update();
                event->accept();
                return;
            }
        }
    }

    QWidget::wheelEvent(event);
}

std::optional<int>
stream_cell::line_edit_vertex_at(const QPointF& pos_px) const {
    if (!line_edit_preview_.has_value()) {
        return std::nullopt;
    }

    double best_distance = std::max<double>(
        stream_cell_support::edit_vertex_hit_radius_px,
        line_width_visual_value(line_edit_preview_->width_text) + 7.0
    );
    std::optional<int> best_index;

    for (int index = 0;
         index < static_cast<int>(line_edit_preview_->pts_pct.size());
         index += 1) {
        const QPointF point_px = to_px(line_edit_preview_->pts_pct.at(
            static_cast<std::vector<QPointF>::size_type>(index)
        ));
        const double distance = QLineF(pos_px, point_px).length();
        if (distance > best_distance) {
            continue;
        }

        best_distance = distance;
        best_index = index;
    }

    return best_index;
}

std::optional<stream_cell::line_edit_segment_hit_info>
stream_cell::line_edit_segment_hit(const QPointF& pos_px) const {
    if (!line_edit_preview_.has_value()
        || line_edit_preview_->pts_pct.size() < 2) {
        return std::nullopt;
    }

    const auto& points = line_edit_preview_->pts_pct;
    double best_distance = std::max<double>(
        stream_cell_support::edit_segment_hit_radius_px,
        line_width_visual_value(line_edit_preview_->width_text) + 7.0
    );
    std::optional<line_edit_segment_hit_info> best_hit;

    const auto consider_segment = [&](const int visible_segment_index,
                                      const QPointF& a_pct,
                                      const QPointF& b_pct) {
        const QPointF a_px = to_px(a_pct);
        const QPointF b_px = to_px(b_pct);
        const double distance = stream_cell_support::point_segment_distance_px(
            pos_px, a_px, b_px
        );
        if (distance > best_distance) {
            return;
        }

        best_distance = distance;
        best_hit = line_edit_segment_hit_info {
            .visible_segment_index = visible_segment_index,
            .point_pct = clamp_pct(to_pct(
                stream_cell_support::nearest_segment_point(pos_px, a_px, b_px)
            )),
        };
    };

    for (int index = 1; index < static_cast<int>(points.size()); index += 1) {
        consider_segment(
            index - 1,
            points.at(static_cast<std::vector<QPointF>::size_type>(index - 1)),
            points.at(static_cast<std::vector<QPointF>::size_type>(index))
        );
    }

    if (line_edit_preview_->closed && points.size() >= 3) {
        consider_segment(
            static_cast<int>(points.size()) - 1, points.back(), points.front()
        );
    }

    return best_hit;
}

void stream_cell::reset_line_edit_drag_state() {
    line_edit_interaction_.reset_press();
}

void stream_cell::update_line_edit_cursor(const QPointF& pos_px) {
    if (!line_edit_preview_.has_value()) {
        return;
    }

    setCursor(line_edit_interaction_.cursor_shape(
        line_edit_vertex_at(pos_px).has_value(),
        line_edit_segment_hit(pos_px).has_value()
    ));
}

void stream_cell::sync_mouse_tracking() {
    setMouseTracking(drawing_enabled || line_edit_preview_.has_value());
    if (line_edit_interaction_.press_active()) {
        return;
    }
    if (line_edit_preview_.has_value() || drawing_enabled) {
        setCursor(Qt::CrossCursor);
    } else {
        unsetCursor();
    }
}
