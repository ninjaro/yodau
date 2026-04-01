#include "shell/active_edit_controller.hpp"

#include "shell/active_edit_session.hpp"
#include "shell/stream_widget_bridge.hpp"
#include "widgets/stream_cell.hpp"

#include <QColor>

active_edit_controller::active_edit_controller(
    active_edit_session& edit_session, stream_widget_bridge& widget_bridge
)
    : edit_session_(edit_session)
    , widget_bridge_(widget_bridge) {}

void active_edit_controller::initialize_editor_state() const {
    widget_bridge_.initialize_editor_state(edit_session_);
}

void active_edit_controller::set_drawing_new_mode(const bool drawing_new) const {
    edit_session_.set_drawing_new_mode(drawing_new);

    auto* cell = widget_bridge_.active_cell();
    if (cell == nullptr) {
        return;
    }

    cell->clear_draft();
    cell->set_drawing_enabled(drawing_new);

    if (drawing_new) {
        const line_profile& draft_line_profile = edit_session_.draft_line_profile();
        cell->set_draft_params(
            draft_line_profile.name, draft_line_profile.color,
            draft_line_profile.closed, draft_line_profile.color_mode_id,
            draft_line_profile.width_text, draft_line_profile.length_text,
            draft_line_profile.response_text
        );
        return;
    }

    widget_bridge_.sync_active_template_editor(edit_session_);
    widget_bridge_.apply_template_preview(
        edit_session_.active_template_settings().template_name, edit_session_
    );
}

const line_profile& active_edit_controller::apply_line_profile(
    line_profile profile_value
) const {
    edit_session_.set_draft_line_profile(std::move(profile_value));
    const line_profile& draft_line_profile = edit_session_.draft_line_profile();

    widget_bridge_.sync_active_line_editor(edit_session_);

    if (auto* cell = widget_bridge_.active_cell()) {
        cell->set_draft_params(
            draft_line_profile.name, draft_line_profile.color,
            draft_line_profile.closed, draft_line_profile.color_mode_id,
            draft_line_profile.width_text, draft_line_profile.length_text,
            draft_line_profile.response_text
        );
    }

    return draft_line_profile;
}

const template_apply_settings& active_edit_controller::apply_template_settings(
    template_apply_settings settings_value
) const {
    const QString previous_template_name
        = edit_session_.active_template_settings().template_name;
    const bool inherit_template_profile
        = settings_value.template_name.trimmed() != previous_template_name;
    settings_value = edit_session_.resolved_template_settings(
        std::move(settings_value), inherit_template_profile
    );
    edit_session_.set_active_template_settings(settings_value);

    widget_bridge_.sync_active_template_editor(edit_session_);

    if (!edit_session_.drawing_new_mode()) {
        widget_bridge_.apply_template_preview(
            edit_session_.active_template_settings().template_name,
            edit_session_
        );
    }

    return edit_session_.active_template_settings();
}

line_edit_request active_edit_controller::apply_line_edit_preview(
    line_edit_request request
) const {
    edit_session_.set_active_line_edit(std::move(request));

    if (const auto active_line_edit = edit_session_.active_line_edit();
        active_line_edit.has_value()) {
        widget_bridge_.sync_active_persistent(
            active_line_edit->stream_name, edit_session_
        );
        return *active_line_edit;
    }

    return {};
}

void active_edit_controller::clear_line_edit_preview() const {
    const QString stream_name = edit_session_.active_line_edit().has_value()
        ? edit_session_.active_line_edit()->stream_name
        : QString();
    edit_session_.clear_active_line_edit();
    if (!stream_name.isEmpty()) {
        widget_bridge_.sync_active_persistent(stream_name, edit_session_);
    }
}

void active_edit_controller::reset_after_line_saved(
    const QString& final_name
) const {
    if (auto* cell = widget_bridge_.active_cell()) {
        cell->clear_draft();
        cell->set_draft_params(
            QString(), QColor(Qt::red), false, default_line_color_mode_id(),
            default_line_width_text(), default_line_length_text(),
            default_line_response_text()
        );
    }

    edit_session_.reset_draft_line_profile();
    widget_bridge_.sync_active_line_editor(edit_session_, true);
    widget_bridge_.add_template_candidate(final_name);
    widget_bridge_.sync_active_template_editor(edit_session_, true);
}

void active_edit_controller::reset_after_line_edit_saved() const {
    clear_line_edit_preview();
}

void active_edit_controller::reset_after_template_applied() const {
    if (auto* cell = widget_bridge_.active_cell()) {
        cell->clear_draft();
    }

    widget_bridge_.sync_active_template_editor(edit_session_, true);
}

void active_edit_controller::undo_last_draft_point() const {
    auto* cell = widget_bridge_.active_cell();
    if (cell == nullptr) {
        return;
    }

    auto pts = cell->draft_points_pct();
    if (pts.empty()) {
        return;
    }

    pts.pop_back();
    cell->set_draft_points_pct(pts);
}
