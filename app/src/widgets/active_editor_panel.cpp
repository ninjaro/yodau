#include "widgets/active_editor_panel.hpp"

#include "widgets/active_stream_panel.hpp"
#include "widgets/line_profile_panel.hpp"
#include "widgets/template_apply_panel.hpp"
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTreeWidget>

#include <QMetaObject>
#include <QTreeWidgetItem>

#include <algorithm>

active_editor_panel::active_editor_panel(QWidget* parent)
    : QWidget(parent) {
    build_ui();
    set_line_profile(line_profile {});
    set_template_settings(template_apply_settings {});
    refresh_line_list();
}

void active_editor_panel::set_active_candidates(
    const QStringList& names
) const {
    if (active_stream_panel_widget == nullptr) {
        return;
    }

    active_stream_panel_widget->set_active_candidates(names);
    update_tools();
}

void active_editor_panel::set_active_current(const QString& name) const {
    if (active_stream_panel_widget == nullptr) {
        return;
    }

    active_stream_panel_widget->set_active_current(name);
    const auto active_name
        = active_stream_panel_widget->current_stream_settings().stream_name;
    if (edit_stream_name_ != active_name) {
        auto* self = const_cast<active_editor_panel*>(this);
        self->edit_stream_name_ = active_name;
        // Geometry names are only unique within a stream. Never carry an
        // unfinished edit to another stream with a matching name.
        line_edit_combo_->setCurrentIndex(0);
    }
    update_tools();
}

void active_editor_panel::set_active_lines(
    const std::vector<stream_cell::line_instance>& lines
) {
    const QString previous_source_name = line_edit_source_name_;
    active_lines_ = lines;
    refresh_line_list();
    refresh_line_edit_candidates();
    if (line_edit_source_name_ != previous_source_name) {
        reset_edit_points();
    }
    refresh_line_edit_table();
    if (!previous_source_name.isEmpty() && line_edit_source_name_.isEmpty()) {
        emit line_edit_preview_cleared();
    }
    update_tools();
}

void active_editor_panel::add_template_candidate(const QString& name) const {
    if (active_template_panel == nullptr || name.isEmpty()) {
        return;
    }

    active_template_panel->add_template_candidate(name);
    update_tools();
}

void active_editor_panel::set_template_candidates(
    const QStringList& names
) const {
    if (active_template_panel == nullptr) {
        return;
    }

    active_template_panel->set_template_candidates(names);
    update_tools();
}

void active_editor_panel::set_line_profile(const line_profile& profile) {
    if (active_line_panel == nullptr) {
        return;
    }

    active_line_panel->set_line_profile(profile);
    refresh_canvas_tools();
}

line_profile active_editor_panel::current_line_profile() const {
    return active_line_panel != nullptr
        ? active_line_panel->current_line_profile()
        : line_profile {};
}

void active_editor_panel::set_template_settings(
    const template_apply_settings& settings_value
) {
    if (active_template_panel == nullptr) {
        return;
    }

    active_template_panel->set_template_settings(settings_value);
}

template_apply_settings active_editor_panel::current_template_settings() const {
    return active_template_panel != nullptr
        ? active_template_panel->current_template_settings()
        : template_apply_settings {};
}

void active_editor_panel::reset_line_form() {
    if (active_line_panel == nullptr) {
        return;
    }

    active_line_panel->reset_form();
    emit line_profile_changed(current_line_profile());
}

void active_editor_panel::reset_template_form() {
    if (active_template_panel == nullptr) {
        return;
    }

    active_template_panel->reset_form();
    emit template_settings_changed(current_template_settings());
}

void active_editor_panel::set_line_closed(const bool closed) const {
    if (active_line_panel == nullptr) {
        return;
    }

    active_line_panel->set_line_closed(closed);
}

QString active_editor_panel::current_template_name() const {
    if (active_template_panel == nullptr) {
        return {};
    }

    return active_template_panel->current_template_name();
}

QColor active_editor_panel::preview_color() const {
    return active_template_panel != nullptr
        ? active_template_panel->preview_color()
        : QColor(Qt::red);
}

void active_editor_panel::update_tools() const {
    if (active_stream_panel_widget == nullptr) {
        return;
    }

    const bool has_active = active_stream_panel_widget->has_active_stream();
    const bool drawing_mode = active_stream_panel_widget->drawing_new_mode();

    if (editor_tabs != nullptr) {
        editor_tabs->setEnabled(has_active);
    }

    if (active_line_panel != nullptr) {
        active_line_panel->set_panel_active(has_active && drawing_mode);
    }

    if (active_template_panel != nullptr) {
        const bool has_templates
            = active_template_panel->has_template_candidates();
        active_template_panel->set_panel_active(
            has_active && has_templates && !drawing_mode
        );
    }

    refresh_status_summary();
    refresh_line_summary();
    refresh_line_action_state();
    refresh_line_edit_state();
}

void active_editor_panel::refresh_status_summary() const {
    if (status_summary_label == nullptr
        || active_stream_panel_widget == nullptr) {
        return;
    }

    if (!active_stream_panel_widget->has_active_stream()) {
        status_summary_label->setText(QStringLiteral(
            "Activate a visible stream to draw lines, apply templates, "
            "enable, detach, and edit saved lines, or save edited variants."
        ));
        return;
    }

    const QString mode_text = active_stream_panel_widget->drawing_new_mode()
        ? QStringLiteral("draw new")
        : QStringLiteral("use template");
    const int total_lines = static_cast<int>(active_lines_.size());
    const int enabled_lines = static_cast<int>(std::count_if(
        active_lines_.cbegin(), active_lines_.cend(),
        [](const stream_cell::line_instance& line_value) {
            return line_value.enabled;
        }
    ));
    const QString template_text = active_template_panel != nullptr
            && active_template_panel->has_template_candidates()
        ? QStringLiteral("templates ready")
        : QStringLiteral("no templates yet");

    status_summary_label->setText(
        QStringLiteral("%1 | %2 | %3 enabled of %4 | %5")
            .arg(active_stream_panel_widget->current_stream_settings()
                     .stream_name)
            .arg(mode_text)
            .arg(enabled_lines)
            .arg(total_lines)
            .arg(template_text)
    );
}

void active_editor_panel::refresh_line_list() const {
    if (line_list_widget == nullptr) {
        return;
    }

    const QString selected_name = selected_line_name();
    line_list_widget->blockSignals(true);
    line_list_widget->clear();

    for (const auto& line_value : active_lines_) {
        const auto item = new QTreeWidgetItem(line_list_widget);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(
            0, line_value.enabled ? Qt::Checked : Qt::Unchecked
        );
        item->setText(1, line_value.template_name);
        item->setData(1, Qt::UserRole, line_value.template_name);
        item->setText(
            2,
            QStringLiteral("%1 | mode=%2")
                .arg(line_profile_summary_text(
                    line_value.width_text, line_value.length_text,
                    line_value.response_text
                ))
                .arg(normalized_line_color_mode_id(line_value.color_mode_id))
        );

        if (!selected_name.isEmpty()
            && line_value.template_name == selected_name) {
            line_list_widget->setCurrentItem(item);
        }
    }

    line_list_widget->blockSignals(false);
    refresh_line_summary();
}

void active_editor_panel::refresh_line_summary() const {
    if (line_summary_label == nullptr
        || active_stream_panel_widget == nullptr) {
        return;
    }

    if (!active_stream_panel_widget->has_active_stream()) {
        line_summary_label->setText(
            QStringLiteral("No active stream selected for line routing.")
        );
        return;
    }

    const int total_lines = static_cast<int>(active_lines_.size());
    const int enabled_lines = static_cast<int>(std::count_if(
        active_lines_.cbegin(), active_lines_.cend(),
        [](const stream_cell::line_instance& line_value) {
            return line_value.enabled;
        }
    ));

    if (total_lines == 0) {
        line_summary_label->setText(QStringLiteral(
            "No saved lines on this stream yet. Draw a new line or apply "
            "a template from the draw tab."
        ));
        refresh_line_action_state();
        return;
    }

    line_summary_label->setText(
        QStringLiteral(
            "%1 saved lines | %2 enabled | %3 disabled | select one to detach"
        )
            .arg(total_lines)
            .arg(enabled_lines)
            .arg(std::max(0, total_lines - enabled_lines))
    );
    refresh_line_action_state();
}

void active_editor_panel::refresh_line_action_state() const {
    if (line_detach_button_ == nullptr
        || active_stream_panel_widget == nullptr) {
        return;
    }

    line_detach_button_->setEnabled(
        active_stream_panel_widget->has_active_stream()
        && !selected_line_name().trimmed().isEmpty()
    );
}

void active_editor_panel::on_line_item_changed(
    QTreeWidgetItem* item, const int column
) {
    if (item == nullptr || column != 0) {
        return;
    }

    queue_line_enabled(
        item->data(1, Qt::UserRole).toString(),
        item->checkState(0) == Qt::Checked
    );
}

void active_editor_panel::on_line_selection_changed() {
    refresh_line_action_state();
}

void active_editor_panel::on_detach_selected_line_clicked() {
    const QString line_name = selected_line_name().trimmed();
    if (line_name.isEmpty()) {
        return;
    }

    emit line_detach_requested(line_name);
}

void active_editor_panel::on_editor_tab_changed(const int index) {
    Q_UNUSED(index);

    if (editor_tabs == nullptr || edit_tab_widget_ == nullptr) {
        return;
    }

    refresh_inspector();

    if (editor_tabs->currentWidget() == edit_tab_widget_) {
        if (!line_edit_source_name_.isEmpty()) {
            emit_line_preview_if_visible();
        } else {
            emit line_edit_preview_cleared();
        }
        refresh_canvas_tools();
        return;
    }

    emit line_edit_preview_cleared();
    refresh_canvas_tools();
}

void active_editor_panel::queue_line_enabled(
    QString line_name, const bool enabled
) {
    pending_line_toggles_.emplace_back(std::move(line_name), enabled);
    if (line_toggle_flush_scheduled_) {
        return;
    }

    line_toggle_flush_scheduled_ = true;

    QMetaObject::invokeMethod(
        this,
        [this]() {
            line_toggle_flush_scheduled_ = false;
            const auto queued_toggles = pending_line_toggles_;
            pending_line_toggles_.clear();

            for (const auto& [queued_line_name, queued_enabled] :
                 queued_toggles) {
                emit line_enabled_changed(queued_line_name, queued_enabled);
            }
        },
        Qt::QueuedConnection
    );
}

QString active_editor_panel::selected_line_name() const {
    if (line_list_widget == nullptr
        || line_list_widget->currentItem() == nullptr) {
        return {};
    }

    return line_list_widget->currentItem()->data(1, Qt::UserRole).toString();
}
