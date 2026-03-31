#include "shell/active_editor_bridge.hpp"

#include "shell/active_edit_session.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/settings_panel.hpp"
#include "widgets/stream_board.hpp"
#include "widgets/stream_cell.hpp"

#include <QColor>

active_editor_bridge::active_editor_bridge(
    stream_board* main_zone, settings_panel* settings
)
    : settings_(settings)
    , main_zone_(main_zone)
    , grid_(main_zone != nullptr ? main_zone->grid_mode() : nullptr) {}

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

    settings_->set_active_current(active_name);
    settings_->set_active_stream_settings(settings_value);
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
    cell->clear_draft();
    cell->set_drawing_enabled(edit_session.drawing_new_mode());

    if (edit_session.drawing_new_mode()) {
        const line_profile& draft_line_profile = edit_session.draft_line_profile();
        cell->set_draft_params(
            draft_line_profile.name, draft_line_profile.color,
            draft_line_profile.closed, draft_line_profile.width_text,
            draft_line_profile.length_text, draft_line_profile.response_text
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
        }
        return;
    }

    if (main_zone_ != nullptr) {
        if (auto* cell = main_zone_->active_cell()) {
            cell->set_persistent_lines(edit_session.stream_lines(active_name));
        }
    }

    if (settings_ == nullptr) {
        return;
    }

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
        active_template_settings.width_text,
        active_template_settings.length_text,
        active_template_settings.response_text
    );
    cell->set_draft_points_pct(template_value->pts_pct);
}
