#ifndef YODAU_APP_WIDGETS_LINE_EDIT_INTERACTION_HPP
#define YODAU_APP_WIDGETS_LINE_EDIT_INTERACTION_HPP

#include <QPoint>
#include <QPointF>
#include <Qt>

#include <optional>

class line_edit_interaction final {
public:
    enum class drag_mode {
        none,
        shape,
        point,
    };

    struct press_result {
        bool hit_shape { false };
        std::optional<int> selected_vertex;
        bool selection_cleared { false };
    };

    [[nodiscard]] std::optional<int> selected_vertex() const {
        return selected_vertex_;
    }

    [[nodiscard]] std::optional<int> pressed_vertex() const {
        return pressed_vertex_;
    }

    [[nodiscard]] bool press_active() const { return press_active_; }
    [[nodiscard]] bool press_moved() const { return press_moved_; }
    [[nodiscard]] bool press_hit_shape() const { return press_hit_shape_; }
    [[nodiscard]] drag_mode current_drag_mode() const { return drag_mode_; }
    [[nodiscard]] QPointF press_origin_pct() const { return press_origin_pct_; }

    void sync_preview_selection(
        const int selected_visible_index, const int point_count
    ) {
        if (selected_visible_index >= 0 && selected_visible_index < point_count) {
            selected_vertex_ = selected_visible_index;
            return;
        }

        if (selected_visible_index < 0) {
            selected_vertex_.reset();
            return;
        }

        if (selected_vertex_.has_value()
            && (*selected_vertex_ < 0 || *selected_vertex_ >= point_count)) {
            selected_vertex_.reset();
        }
    }

    void clear_preview() {
        selected_vertex_.reset();
        reset_press();
    }

    void set_selected_vertex(const std::optional<int> vertex_index) {
        selected_vertex_ = vertex_index;
    }

    press_result begin_press(
        const std::optional<int> vertex_index, const bool segment_hit,
        const QPointF& origin_pct, const QPoint& origin_px
    ) {
        pressed_vertex_ = vertex_index;
        press_hit_shape_ = pressed_vertex_.has_value() || segment_hit;
        press_origin_pct_ = origin_pct;
        press_origin_px_ = origin_px;
        press_active_ = true;
        press_moved_ = false;
        drag_mode_ = drag_mode::none;

        press_result result;
        result.hit_shape = press_hit_shape_;
        if (pressed_vertex_.has_value()) {
            selected_vertex_ = pressed_vertex_;
            result.selected_vertex = selected_vertex_;
        } else if (!press_hit_shape_) {
            selected_vertex_.reset();
            result.selection_cleared = true;
        }
        return result;
    }

    [[nodiscard]] bool start_drag_if_threshold(
        const QPoint& current_px, const int threshold_px
    ) {
        if (!press_active_ || !press_hit_shape_ || press_moved_) {
            return false;
        }

        if ((current_px - press_origin_px_).manhattanLength() < threshold_px) {
            return false;
        }

        press_moved_ = true;
        drag_mode_ = pressed_vertex_.has_value() ? drag_mode::point
                                                 : drag_mode::shape;
        return true;
    }

    void update_press_origin(const QPointF& origin_pct, const QPoint& origin_px) {
        press_origin_pct_ = origin_pct;
        press_origin_px_ = origin_px;
    }

    [[nodiscard]] bool release_was_dragging() const {
        return press_active_ && press_moved_;
    }

    void reset_press() {
        pressed_vertex_.reset();
        press_origin_pct_ = {};
        press_origin_px_ = {};
        press_active_ = false;
        press_moved_ = false;
        press_hit_shape_ = false;
        drag_mode_ = drag_mode::none;
    }

    [[nodiscard]] Qt::CursorShape cursor_shape(
        const bool vertex_hit, const bool segment_hit
    ) const {
        if (press_active_ && press_hit_shape_) {
            return Qt::ClosedHandCursor;
        }
        if (vertex_hit) {
            return Qt::SizeAllCursor;
        }
        if (segment_hit) {
            return Qt::OpenHandCursor;
        }
        return Qt::CrossCursor;
    }

private:
    std::optional<int> selected_vertex_;
    std::optional<int> pressed_vertex_;
    QPointF press_origin_pct_;
    QPoint press_origin_px_;
    bool press_active_ { false };
    bool press_moved_ { false };
    bool press_hit_shape_ { false };
    drag_mode drag_mode_ { drag_mode::none };
};

#endif // YODAU_APP_WIDGETS_LINE_EDIT_INTERACTION_HPP
