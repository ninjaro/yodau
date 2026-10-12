#include "widgets/active_editor_panel.hpp"

#include <QLineEdit>
#include <QSignalBlocker>

#include "widgets/active_editor_panel/line_lookup.hpp"

active_editor_panel::edit_snapshot
active_editor_panel::edit_snapshot_now() const {
    return edit_snapshot {
        .points_pct = line_edit_points_pct_,
        .point_enabled = line_edit_point_enabled_,
        .selected_row = line_edit_selected_row_,
    };
}

bool active_editor_panel::snapshot_matches_current(
    const edit_snapshot& snapshot
) const {
    return snapshot.points_pct == line_edit_points_pct_
        && snapshot.point_enabled == line_edit_point_enabled_
        && snapshot.selected_row == line_edit_selected_row_;
}

void active_editor_panel::clear_line_edit_history() {
    line_edit_undo_stack_.clear();
    line_edit_redo_stack_.clear();
    line_edit_transaction_active_ = false;
    refresh_line_edit_state();
}

void active_editor_panel::push_edit_undo() {
    if (line_edit_source_name_.isEmpty() || line_edit_points_pct_.empty()) {
        return;
    }

    const edit_snapshot snapshot = edit_snapshot_now();
    if (!line_edit_undo_stack_.empty()
        && line_edit_undo_stack_.back().points_pct == snapshot.points_pct
        && line_edit_undo_stack_.back().point_enabled == snapshot.point_enabled
        && line_edit_undo_stack_.back().selected_row == snapshot.selected_row) {
        return;
    }

    constexpr size_t max_history_entries = 64;
    line_edit_undo_stack_.push_back(snapshot);
    if (line_edit_undo_stack_.size() > max_history_entries) {
        line_edit_undo_stack_.erase(line_edit_undo_stack_.begin());
    }
    line_edit_redo_stack_.clear();
    refresh_line_edit_state();
}

void active_editor_panel::record_edit() {
    if (line_edit_transaction_active_) {
        return;
    }

    push_edit_undo();
}

void active_editor_panel::restore_line_edit_snapshot(
    const edit_snapshot& snapshot
) {
    line_edit_points_pct_ = snapshot.points_pct;
    line_edit_point_enabled_ = snapshot.point_enabled;
    if (line_edit_point_enabled_.size() != line_edit_points_pct_.size()) {
        line_edit_point_enabled_.assign(line_edit_points_pct_.size(), true);
    }

    line_edit_selected_row_ = snapshot.selected_row >= 0
            && snapshot.selected_row
                < static_cast<int>(line_edit_points_pct_.size())
        ? snapshot.selected_row
        : -1;

    refresh_edit_view();
}

void active_editor_panel::refresh_edit_view() {
    refresh_line_edit_table();
    emit_line_preview_if_visible();
}

bool active_editor_panel::line_edit_has_unsaved_changes() const {
    const auto source_line = active_editor_panel_support::find_line(
        active_lines_, line_edit_source_name_
    );
    if (!source_line.has_value()) {
        return false;
    }

    if (line_edit_points_pct_ != source_line->pts_pct) {
        return true;
    }

    const std::vector<bool> all_points_enabled(
        line_edit_points_pct_.size(), true
    );
    if (line_edit_point_enabled_ != all_points_enabled) {
        return true;
    }

    return line_edit_name_edit_ != nullptr
        && line_edit_name_edit_->text().trimmed()
        != active_editor_panel_support::variant_name(
               source_line->template_name
        );
}

void active_editor_panel::begin_line_edit_change() {
    if (line_edit_transaction_active_) {
        return;
    }

    push_edit_undo();
    line_edit_transaction_active_ = true;
}

void active_editor_panel::finish_line_edit_change() {
    line_edit_transaction_active_ = false;
    if (!line_edit_undo_stack_.empty()
        && snapshot_matches_current(line_edit_undo_stack_.back())) {
        line_edit_undo_stack_.pop_back();
    }
    refresh_line_edit_state();
}

bool active_editor_panel::undo_line_edit_change() {
    if (line_edit_undo_stack_.empty()) {
        return false;
    }

    line_edit_redo_stack_.push_back(edit_snapshot_now());
    const edit_snapshot snapshot = line_edit_undo_stack_.back();
    line_edit_undo_stack_.pop_back();
    line_edit_transaction_active_ = false;
    restore_line_edit_snapshot(snapshot);
    refresh_line_edit_state();
    return true;
}

bool active_editor_panel::redo_line_edit_change() {
    if (line_edit_redo_stack_.empty()) {
        return false;
    }

    line_edit_undo_stack_.push_back(edit_snapshot_now());
    const edit_snapshot snapshot = line_edit_redo_stack_.back();
    line_edit_redo_stack_.pop_back();
    line_edit_transaction_active_ = false;
    restore_line_edit_snapshot(snapshot);
    refresh_line_edit_state();
    return true;
}

bool active_editor_panel::revert_line_edit_changes() {
    const auto source_line = active_editor_panel_support::find_line(
        active_lines_, line_edit_source_name_
    );
    if (!source_line.has_value()) {
        return false;
    }

    line_edit_points_pct_ = source_line->pts_pct;
    line_edit_point_enabled_.assign(line_edit_points_pct_.size(), true);
    line_edit_selected_row_ = -1;
    if (line_edit_name_edit_ != nullptr) {
        QSignalBlocker blocker(line_edit_name_edit_);
        line_edit_name_edit_->setText(
            active_editor_panel_support::variant_name(
                source_line->template_name
            )
        );
    }

    clear_line_edit_history();
    refresh_edit_view();
    return true;
}
