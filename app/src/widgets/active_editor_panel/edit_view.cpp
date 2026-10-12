#include "widgets/active_editor_panel.hpp"

#include "shell/str_label.hpp"
#include "widgets/active_editor_panel/line_lookup.hpp"
#include "widgets/active_editor_panel/point_rows.hpp"
#include "widgets/active_stream_panel.hpp"
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>

#include <QSignalBlocker>
#include <QTableWidgetItem>

#include <algorithm>
#include <cmath>
#include <limits>

namespace active_editor_panel_support {

QString none_text() { return str_label("none"); }

} // namespace active_editor_panel_support

void active_editor_panel::refresh_line_edit_candidates() {
    if (line_edit_combo_ == nullptr) {
        return;
    }

    const QString previous_source_name = line_edit_source_name_;
    QSignalBlocker blocker(line_edit_combo_);
    line_edit_combo_->clear();
    line_edit_combo_->addItem(
        active_editor_panel_support::none_text(), QVariant()
    );

    for (const auto& line_value : active_lines_) {
        if (!line_value.enabled) {
            continue;
        }

        line_edit_combo_->addItem(
            line_value.template_name, line_value.template_name
        );
    }

    const int selected_index = previous_source_name.isEmpty()
        ? 0
        : line_edit_combo_->findData(previous_source_name);
    line_edit_combo_->setCurrentIndex(selected_index >= 0 ? selected_index : 0);
    line_edit_source_name_
        = line_edit_combo_->currentData().toString().trimmed();
}

void active_editor_panel::reset_edit_points() {
    line_edit_points_pct_.clear();
    line_edit_point_enabled_.clear();
    line_edit_selected_row_ = -1;
    clear_line_edit_history();

    const auto line_value = active_editor_panel_support::find_line(
        active_lines_, line_edit_source_name_
    );
    if (!line_value.has_value()) {
        return;
    }

    line_edit_points_pct_ = line_value->pts_pct;
    line_edit_point_enabled_.assign(line_edit_points_pct_.size(), true);
}

void active_editor_panel::refresh_line_edit_table() {
    if (line_edit_points_table_ == nullptr) {
        return;
    }

    syncing_line_edit_ui_ = true;
    line_edit_points_table_->blockSignals(true);
    line_edit_points_table_->clearContents();
    line_edit_points_table_->setRowCount(0);

    const auto line_value = active_editor_panel_support::find_line(
        active_lines_, line_edit_source_name_
    );
    if (!line_value.has_value()) {
        line_edit_points_pct_.clear();
        line_edit_point_enabled_.clear();
        line_edit_selected_row_ = -1;
        clear_line_edit_history();
        line_edit_points_table_->blockSignals(false);
        syncing_line_edit_ui_ = false;
        refresh_line_edit_summary();
        refresh_line_edit_state();
        return;
    }

    if (line_edit_points_pct_.empty() && !line_value->pts_pct.empty()) {
        reset_edit_points();
    } else if (
        line_edit_point_enabled_.size() != line_edit_points_pct_.size()
    ) {
        line_edit_point_enabled_.assign(line_edit_points_pct_.size(), true);
    }

    line_edit_points_table_->setRowCount(
        static_cast<int>(line_edit_points_pct_.size())
    );
    for (int row = 0; row < static_cast<int>(line_edit_points_pct_.size());
         row += 1) {
        const auto point_index
            = static_cast<std::vector<QPointF>::size_type>(row);
        const QPointF& point_value = line_edit_points_pct_.at(point_index);
        const auto enabled_item = new QTableWidgetItem;
        enabled_item->setFlags(
            (enabled_item->flags() | Qt::ItemIsUserCheckable)
            & ~Qt::ItemIsEditable
        );
        enabled_item->setCheckState(
            line_edit_point_enabled_.at(
                static_cast<std::vector<bool>::size_type>(row)
            )
                ? Qt::Checked
                : Qt::Unchecked
        );
        line_edit_points_table_->setItem(row, 0, enabled_item);

        const auto index_item = new QTableWidgetItem(QString::number(row + 1));
        index_item->setFlags(index_item->flags() & ~Qt::ItemIsEditable);
        line_edit_points_table_->setItem(row, 1, index_item);

        const auto x_item
            = new QTableWidgetItem(QString::number(point_value.x(), 'f', 2));
        line_edit_points_table_->setItem(row, 2, x_item);

        const auto y_item
            = new QTableWidgetItem(QString::number(point_value.y(), 'f', 2));
        line_edit_points_table_->setItem(row, 3, y_item);
    }

    if (line_edit_selected_row_ >= 0
        && line_edit_selected_row_ < line_edit_points_table_->rowCount()) {
        line_edit_points_table_->selectRow(line_edit_selected_row_);
    } else {
        line_edit_points_table_->clearSelection();
    }

    line_edit_points_table_->blockSignals(false);
    syncing_line_edit_ui_ = false;
    refresh_line_edit_summary();
    refresh_line_edit_state();
}

void active_editor_panel::refresh_line_edit_summary() const {
    if (line_edit_summary_label_ == nullptr) {
        return;
    }

    const auto line_value = active_editor_panel_support::find_line(
        active_lines_, line_edit_source_name_
    );
    if (line_value.has_value()) {
        const int kept_points = static_cast<int>(std::count(
            line_edit_point_enabled_.cbegin(), line_edit_point_enabled_.cend(),
            true
        ));
        const QString selected_text = line_edit_selected_row_ >= 0
                && line_edit_selected_row_
                    < static_cast<int>(line_edit_points_pct_.size())
            ? QStringLiteral(" Selected point row: %1.")
                  .arg(line_edit_selected_row_ + 1)
            : QString();
        line_edit_summary_label_->setText(
            QStringLiteral(
                "Select an enabled line, exclude points you do not want, then "
                "save the edited variant under a new name. "
                "Current source: %1 | keeping %2 of %3 points.%4 "
                "Drag points or the whole line, double-click a segment to "
                "insert a point, use Delete to remove one, and use arrows or "
                "the mouse wheel for small adjustments."
            )
                .arg(line_value->template_name)
                .arg(kept_points)
                .arg(line_edit_points_pct_.size())
                .arg(selected_text)
        );
        return;
    }

    line_edit_summary_label_->setText(QStringLiteral(
        "Select an enabled line for this stream to preview it as a dashed "
        "editable variant, then adjust points and save it under a new name."
    ));
}

void active_editor_panel::refresh_line_edit_state() const {
    if (line_edit_combo_ == nullptr || line_edit_points_table_ == nullptr
        || line_edit_name_edit_ == nullptr || line_edit_save_button_ == nullptr
        || line_edit_undo_button_ == nullptr
        || line_edit_redo_button_ == nullptr
        || line_edit_revert_button_ == nullptr
        || line_edit_delete_button_ == nullptr || editor_tabs == nullptr) {
        return;
    }

    const bool has_active = active_stream_panel_widget != nullptr
        && active_stream_panel_widget->has_active_stream();
    const int enabled_line_count = static_cast<int>(std::count_if(
        active_lines_.cbegin(), active_lines_.cend(),
        [](const stream_cell::line_instance& line_value) {
            return line_value.enabled;
        }
    ));
    const bool has_selected_line = !line_edit_source_name_.trimmed().isEmpty();
    const int kept_points = static_cast<int>(std::count(
        line_edit_point_enabled_.cbegin(), line_edit_point_enabled_.cend(), true
    ));
    const bool can_save = has_active && has_selected_line
        && !line_edit_name_edit_->text().trimmed().isEmpty()
        && kept_points >= 2;
    const bool has_selected_row = line_edit_selected_row_ >= 0
        && line_edit_selected_row_
            < static_cast<int>(line_edit_points_pct_.size());
    const bool selected_row_enabled = has_selected_row
        && line_edit_selected_row_
            < static_cast<int>(line_edit_point_enabled_.size())
        && line_edit_point_enabled_.at(
            static_cast<std::vector<bool>::size_type>(line_edit_selected_row_)
        );
    const bool can_delete_selected = has_active && has_selected_line
        && has_selected_row && (!selected_row_enabled || kept_points > 2);

    line_edit_combo_->setEnabled(has_active && enabled_line_count > 0);
    line_edit_points_table_->setEnabled(has_active && has_selected_line);
    line_edit_name_edit_->setEnabled(has_active && has_selected_line);
    line_edit_undo_button_->setEnabled(
        has_active && has_selected_line && !line_edit_undo_stack_.empty()
    );
    line_edit_redo_button_->setEnabled(
        has_active && has_selected_line && !line_edit_redo_stack_.empty()
    );
    line_edit_revert_button_->setEnabled(
        has_active && has_selected_line && line_edit_has_unsaved_changes()
    );
    line_edit_delete_button_->setEnabled(can_delete_selected);
    line_edit_save_button_->setEnabled(can_save);

    const int edit_tab_index = editor_tabs->indexOf(edit_tab_widget_);
    if (edit_tab_index >= 0) {
        editor_tabs->setTabEnabled(
            edit_tab_index, has_active && enabled_line_count > 0
        );
    }
    refresh_canvas_tools();
    refresh_inspector();
}

void active_editor_panel::emit_line_preview_if_visible() {
    if (editor_tabs != nullptr
        && editor_tabs->currentWidget() == edit_tab_widget_
        && !line_edit_source_name_.isEmpty()) {
        emit line_edit_preview_changed(current_line_edit_request());
    }
}

line_edit_request active_editor_panel::current_line_edit_request() const {
    line_edit_request request;
    if (active_stream_panel_widget == nullptr) {
        return request;
    }

    const auto line_value = active_editor_panel_support::find_line(
        active_lines_, line_edit_source_name_
    );
    if (!line_value.has_value()) {
        return request;
    }

    request.stream_name
        = active_stream_panel_widget->current_stream_settings().stream_name;
    request.source_line_name = line_value->template_name;
    request.profile = line_profile {
        .name = line_edit_name_edit_ != nullptr
            ? line_edit_name_edit_->text().trimmed()
            : QString(),
        .color = line_value->color,
        .color_mode_id = line_value->color_mode_id,
        .closed = line_value->closed,
        .width_text = line_value->width_text,
        .length_text = line_value->length_text,
        .response_text = line_value->response_text,
    };

    for (int index = 0; index < static_cast<int>(line_edit_points_pct_.size());
         index += 1) {
        if (index >= static_cast<int>(line_edit_point_enabled_.size())
            || !line_edit_point_enabled_.at(
                static_cast<std::vector<bool>::size_type>(index)
            )) {
            continue;
        }
        request.points_pct.push_back(line_edit_points_pct_.at(
            static_cast<std::vector<QPointF>::size_type>(index)
        ));
    }
    if (line_edit_selected_row_ >= 0) {
        request.selected_visible_index
            = active_editor_panel_support::visible_index_for_row(
                  line_edit_point_enabled_, line_edit_selected_row_
            )
                  .value_or(-1);
    }

    return request;
}

void active_editor_panel::on_line_edit_selection_changed(const int index) {
    Q_UNUSED(index);

    if (line_edit_combo_ == nullptr || syncing_line_edit_ui_) {
        return;
    }

    line_edit_source_name_
        = line_edit_combo_->currentData().toString().trimmed();
    reset_edit_points();

    const auto line_value = active_editor_panel_support::find_line(
        active_lines_, line_edit_source_name_
    );
    if (line_value.has_value()) {
        if (line_edit_name_edit_ != nullptr) {
            QSignalBlocker blocker(line_edit_name_edit_);
            line_edit_name_edit_->setText(
                active_editor_panel_support::variant_name(
                    line_value->template_name
                )
            );
        }
    } else if (line_edit_name_edit_ != nullptr) {
        QSignalBlocker blocker(line_edit_name_edit_);
        line_edit_name_edit_->clear();
    }

    refresh_line_edit_table();

    if (line_value.has_value()) {
        emit_line_preview_if_visible();
        return;
    }

    emit line_edit_preview_cleared();
}

void active_editor_panel::on_line_table_selection_changed() {
    if (line_edit_points_table_ == nullptr || syncing_line_edit_ui_) {
        return;
    }

    const int row = line_edit_points_table_->currentRow();
    line_edit_selected_row_
        = row >= 0 && row < static_cast<int>(line_edit_points_pct_.size()) ? row
                                                                           : -1;
    refresh_line_edit_summary();
    refresh_line_edit_state();
    emit_line_preview_if_visible();
}

void active_editor_panel::on_line_point_item_changed(QTableWidgetItem* item) {
    if (item == nullptr || syncing_line_edit_ui_) {
        return;
    }

    const int row = item->row();
    if (row < 0 || row >= static_cast<int>(line_edit_points_pct_.size())
        || line_edit_points_pct_.size() != line_edit_point_enabled_.size()) {
        return;
    }

    if (item->column() == 0) {
        const bool enabled = item->checkState() == Qt::Checked;
        const bool previous_enabled = line_edit_point_enabled_.at(
            static_cast<std::vector<bool>::size_type>(row)
        );
        if (enabled == previous_enabled) {
            return;
        }

        const int kept_points = static_cast<int>(std::count(
            line_edit_point_enabled_.cbegin(), line_edit_point_enabled_.cend(),
            true
        ));
        if (!enabled && kept_points <= 2) {
            refresh_line_edit_table();
            return;
        }

        record_edit();
        line_edit_point_enabled_.at(
            static_cast<std::vector<bool>::size_type>(row)
        ) = enabled;
        if (!enabled && line_edit_selected_row_ == row) {
            line_edit_selected_row_ = -1;
        }
        refresh_edit_view();
        return;
    }

    if (item->column() != 2 && item->column() != 3) {
        return;
    }

    bool ok = false;
    const double value = item->text().trimmed().toDouble(&ok);
    if (!ok) {
        refresh_line_edit_table();
        return;
    }

    QPointF updated_point = line_edit_points_pct_.at(
        static_cast<std::vector<QPointF>::size_type>(row)
    );
    if (item->column() == 2) {
        updated_point.setX(value);
    } else {
        updated_point.setY(value);
    }
    updated_point = active_editor_panel_support::clamped_pct(updated_point);

    const QPointF previous_point = line_edit_points_pct_.at(
        static_cast<std::vector<QPointF>::size_type>(row)
    );
    if (std::abs(previous_point.x() - updated_point.x())
            <= std::numeric_limits<double>::epsilon()
        && std::abs(previous_point.y() - updated_point.y())
            <= std::numeric_limits<double>::epsilon()) {
        refresh_line_edit_table();
        return;
    }

    record_edit();
    line_edit_selected_row_ = row;
    line_edit_points_pct_.at(static_cast<std::vector<QPointF>::size_type>(row))
        = updated_point;
    refresh_edit_view();
}

void active_editor_panel::on_line_name_text_changed(const QString& text) {
    Q_UNUSED(text);

    refresh_line_edit_state();
    emit_line_preview_if_visible();
}

void active_editor_panel::on_line_edit_undo_clicked() {
    undo_line_edit_change();
}

void active_editor_panel::on_line_edit_redo_clicked() {
    redo_line_edit_change();
}

void active_editor_panel::on_line_edit_revert_clicked() {
    revert_line_edit_changes();
}

void active_editor_panel::on_line_edit_delete_clicked() {
    if (line_edit_selected_row_ < 0) {
        return;
    }

    delete_line_edit_row(line_edit_selected_row_);
}

void active_editor_panel::on_line_edit_save_clicked() {
    const line_edit_request request = current_line_edit_request();
    if (request.source_line_name.isEmpty() || request.profile.name.isEmpty()
        || request.points_pct.size() < 2) {
        return;
    }

    emit line_edit_save_requested(request);

    line_edit_source_name_.clear();
    line_edit_points_pct_.clear();
    line_edit_point_enabled_.clear();
    line_edit_selected_row_ = -1;
    clear_line_edit_history();
    if (line_edit_combo_ != nullptr) {
        QSignalBlocker blocker(line_edit_combo_);
        line_edit_combo_->setCurrentIndex(0);
    }
    if (line_edit_name_edit_ != nullptr) {
        QSignalBlocker blocker(line_edit_name_edit_);
        line_edit_name_edit_->clear();
    }
    refresh_line_edit_table();
    emit line_edit_preview_cleared();
}
