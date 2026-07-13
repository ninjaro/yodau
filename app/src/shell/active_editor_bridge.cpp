#include "shell/active_editor_bridge.hpp"

#include "shell/active_edit_session.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/settings_panel.hpp"
#include "widgets/stream_board.hpp"
#include "widgets/stream_cell.hpp"

#include <QColor>

#include <algorithm>

namespace active_editor_bridge_support {

std::vector<stream_cell::line_instance> filtered_active_lines(
    const QString& active_name, const active_edit_session& edit_session
) {
    const auto edit_request = edit_session.active_line_edit();
    if (!edit_request.has_value() || edit_request->stream_name != active_name
        || edit_request->source_line_name.isEmpty()) {
        return edit_session.stream_lines(active_name);
    }

    std::vector<stream_cell::line_instance> filtered;
    for (const auto& line_value : edit_session.stream_lines(active_name)) {
        if (line_value.template_name.trimmed()
            == edit_request->source_line_name.trimmed()) {
            continue;
        }
        filtered.push_back(line_value);
    }
    return filtered;
}

std::optional<stream_cell::line_instance> active_line_edit_preview(
    const QString& active_name, const active_edit_session& edit_session
) {
    const auto edit_request = edit_session.active_line_edit();
    if (!edit_request.has_value() || edit_request->stream_name != active_name
        || edit_request->source_line_name.isEmpty()
        || edit_request->points_pct.isEmpty()) {
        return std::nullopt;
    }

    const auto source_line = edit_session.find_stream_line(
        active_name, edit_request->source_line_name
    );
    if (!source_line.has_value()) {
        return std::nullopt;
    }

    stream_cell::line_instance preview_line = *source_line;
    preview_line.template_name = edit_request->profile.name;
    preview_line.color = edit_request->profile.color;
    preview_line.color_mode_id = edit_request->profile.color_mode_id;
    preview_line.closed = edit_request->profile.closed;
    preview_line.width_text = edit_request->profile.width_text;
    preview_line.length_text = edit_request->profile.length_text;
    preview_line.response_text = edit_request->profile.response_text;
    preview_line.selected_visible_index = edit_request->selected_visible_index;
    preview_line.pts_pct.reserve(
        static_cast<std::vector<QPointF>::size_type>(
            edit_request->points_pct.size()
        )
    );
    for (const QPointF& point_value : edit_request->points_pct) {
        preview_line.pts_pct.push_back(point_value);
    }
    return preview_line;
}

std::optional<stream_cell::line_instance> active_line_edit_source(
    const QString& active_name, const active_edit_session& edit_session
) {
    const auto edit_request = edit_session.active_line_edit();
    if (!edit_request.has_value() || edit_request->stream_name != active_name
        || edit_request->source_line_name.isEmpty()) {
        return std::nullopt;
    }

    return edit_session.find_stream_line(
        active_name, edit_request->source_line_name
    );
}

} // namespace active_editor_bridge_support

active_editor_bridge::active_editor_bridge(
    stream_board* main_zone, settings_panel* settings
)
    : settings_(settings)
    , main_zone_(main_zone)
    , grid_(main_zone != nullptr ? main_zone->grid_mode() : nullptr) { }

void active_editor_bridge::initialize_editor_state(
    const active_edit_session& edit_session
) const {
    sync_active_line_editor(edit_session);
    sync_active_template_editor(edit_session);
}

void active_editor_bridge::sync_active_line_editor(
    const active_edit_session& edit_session, const bool reset_form
) const {
    if (settings_ == nullptr) {
        return;
    }

    settings_->set_active_line_profile(edit_session.draft_line_profile());
    if (reset_form) {
        settings_->reset_active_line_form();
    }
}

void active_editor_bridge::sync_active_template_editor(
    const active_edit_session& edit_session, const bool reset_form
) const {
    if (settings_ == nullptr) {
        return;
    }

    settings_->set_active_template_settings(
        edit_session.active_template_settings()
    );
    if (reset_form) {
        settings_->reset_active_template_form();
    }
}

void active_editor_bridge::add_template_candidate(const QString& name) const {
    if (settings_ == nullptr || name.isEmpty()) {
        return;
    }

    settings_->add_template_candidate(name);
}

void active_editor_bridge::sync_active_candidates() const {
    if (settings_ == nullptr || grid_ == nullptr) {
        return;
    }

    settings_->set_active_candidates(grid_->stream_names());
}

void active_editor_bridge::sync_active_selection(
    const QString& active_name, const stream_settings& settings_value
) const {
    if (settings_ == nullptr) {
        return;
    }

    Q_UNUSED(settings_value);
    settings_->set_active_current(active_name);
}

void active_editor_bridge::apply_active_stream(
    const QString& active_name, const stream_settings& settings_value,
    const active_edit_session& edit_session
) const {
    sync_active_selection(active_name, settings_value);

    if (main_zone_ == nullptr) {
        return;
    }

    if (active_name.isEmpty()) {
        main_zone_->clear_active();
    } else {
        main_zone_->set_active_stream(active_name);
    }

    auto* cell = main_zone_->active_cell();
    if (cell == nullptr) {
        return;
    }

    cell->set_stream_settings(settings_value);
    cell->set_labels_enabled(settings_value.labels_enabled);
    if (settings_ != nullptr) {
        QObject::connect(
            cell, &stream_cell::line_edit_point_selected, settings_,
            &settings_panel::select_active_line_edit_point, Qt::UniqueConnection
        );
        QObject::connect(
            cell, &stream_cell::line_edit_shape_drag_requested, settings_,
            &settings_panel::translate_active_line_edit_shape,
            Qt::UniqueConnection
        );
        QObject::connect(
            cell, &stream_cell::line_edit_point_move_requested, settings_,
            &settings_panel::move_active_line_edit_point, Qt::UniqueConnection
        );
        QObject::connect(
            cell, &stream_cell::line_edit_point_split_requested, settings_,
            &settings_panel::split_active_line_edit_point, Qt::UniqueConnection
        );
        QObject::connect(
            cell, &stream_cell::line_edit_segment_insert_requested, settings_,
            &settings_panel::insert_active_line_point_after,
            Qt::UniqueConnection
        );
        QObject::connect(
            cell, &stream_cell::line_edit_point_delete_requested, settings_,
            &settings_panel::delete_active_line_edit_point, Qt::UniqueConnection
        );
        QObject::connect(
            cell, &stream_cell::line_edit_shape_rotate_requested, settings_,
            &settings_panel::rotate_active_line_edit_shape, Qt::UniqueConnection
        );
        QObject::connect(
            cell, &stream_cell::line_edit_change_started, settings_,
            &settings_panel::begin_active_line_edit_change, Qt::UniqueConnection
        );
        QObject::connect(
            cell, &stream_cell::line_edit_change_finished, settings_,
            &settings_panel::finish_active_line_edit_change,
            Qt::UniqueConnection
        );
        QObject::connect(
            cell, &stream_cell::line_edit_undo_requested, settings_,
            &settings_panel::undo_active_line_edit_change, Qt::UniqueConnection
        );
        QObject::connect(
            cell, &stream_cell::line_edit_redo_requested, settings_,
            &settings_panel::redo_active_line_edit_change, Qt::UniqueConnection
        );
        QObject::connect(
            cell, &stream_cell::line_edit_revert_requested, settings_,
            &settings_panel::revert_active_line_edit_changes,
            Qt::UniqueConnection
        );
    }
    const auto line_edit_preview
        = active_editor_bridge_support::active_line_edit_preview(
            active_name, edit_session
        );
    cell->set_line_edit_source_line(
        active_editor_bridge_support::active_line_edit_source(
            active_name, edit_session
        )
    );
    cell->set_line_edit_preview(line_edit_preview);

    if (line_edit_preview.has_value()) {
        cell->set_drawing_enabled(false);
        return;
    }

    cell->clear_draft();
    cell->set_drawing_enabled(edit_session.drawing_new_mode());

    if (edit_session.drawing_new_mode()) {
        const line_profile& draft_line_profile
            = edit_session.draft_line_profile();
        cell->set_draft_params(
            draft_line_profile.name, draft_line_profile.color,
            draft_line_profile.closed, draft_line_profile.color_mode_id,
            draft_line_profile.width_text, draft_line_profile.length_text,
            draft_line_profile.response_text
        );
        return;
    }

    sync_active_template_editor(edit_session);
    apply_template_preview(
        edit_session.active_template_settings().template_name, edit_session
    );
}

void active_editor_bridge::sync_active_persistent(
    const QString& active_name, const active_edit_session& edit_session
) const {
    if (active_name.isEmpty()) {
        if (settings_ != nullptr) {
            settings_->set_template_candidates({});
            settings_->set_active_lines({});
        }
        return;
    }

    if (main_zone_ != nullptr) {
        if (auto* cell = main_zone_->active_cell()) {
            const auto line_edit_preview
                = active_editor_bridge_support::active_line_edit_preview(
                    active_name, edit_session
                );
            cell->set_line_edit_source_line(
                active_editor_bridge_support::active_line_edit_source(
                    active_name, edit_session
                )
            );
            cell->set_persistent_lines(
                active_editor_bridge_support::filtered_active_lines(
                    active_name, edit_session
                )
            );
            cell->set_line_edit_preview(line_edit_preview);
            if (line_edit_preview.has_value()) {
                cell->set_drawing_enabled(false);
            } else {
                cell->set_drawing_enabled(edit_session.drawing_new_mode());
            }
        }
    }

    if (settings_ == nullptr) {
        return;
    }

    settings_->set_active_lines(edit_session.stream_lines(active_name));
    const QSet<QString> used
        = edit_session.used_template_names_for_stream(active_name);
    settings_->set_template_candidates(
        edit_session.template_candidates_excluding(used)
    );
}

void active_editor_bridge::apply_template_preview(
    const QString& template_name, const active_edit_session& edit_session
) const {
    if (main_zone_ == nullptr) {
        return;
    }

    auto* cell = main_zone_->active_cell();
    if (cell == nullptr) {
        return;
    }

    cell->clear_draft();

    const auto template_value = edit_session.template_value(template_name);
    if (template_name.isEmpty() || !template_value.has_value()) {
        return;
    }

    const template_apply_settings& active_template_settings
        = edit_session.active_template_settings();
    const QColor color = active_template_settings.color.isValid()
        ? active_template_settings.color
        : QColor(Qt::red);

    cell->set_draft_params(
        template_name, color, template_value->closed,
        active_template_settings.color_mode_id,
        active_template_settings.width_text,
        active_template_settings.length_text,
        active_template_settings.response_text
    );
    cell->set_draft_points_pct(template_value->pts_pct);
}
