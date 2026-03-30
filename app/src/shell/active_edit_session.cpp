#include "shell/active_edit_session.hpp"

namespace active_edit_session_support {

QString trimmed_name(const QString& value) { return value.trimmed(); }

} // namespace active_edit_session_support

active_edit_session::active_edit_session() { reset_active_template_settings(); }

bool active_edit_session::drawing_new_mode() const { return drawing_new_mode_; }

void active_edit_session::set_drawing_new_mode(const bool drawing_new) {
    drawing_new_mode_ = drawing_new;
}

const line_profile& active_edit_session::draft_line_profile() const {
    return draft_line_profile_;
}

void active_edit_session::set_draft_line_profile(line_profile profile_value) {
    draft_line_profile_ = normalized_line_profile(std::move(profile_value));
}

void active_edit_session::reset_draft_line_profile() {
    draft_line_profile_ = normalized_line_profile(line_profile {});
}

const template_apply_settings&
active_edit_session::active_template_settings() const {
    return active_template_settings_;
}

void active_edit_session::set_active_template_settings(
    template_apply_settings settings_value
) {
    active_template_settings_
        = normalized_template_settings(std::move(settings_value));
}

template_apply_settings active_edit_session::resolved_template_settings(
    template_apply_settings settings_value, const bool inherit_template_profile
) const {
    settings_value = normalized_template_settings(std::move(settings_value));

    if (!inherit_template_profile) {
        return settings_value;
    }

    const auto tpl = template_value(settings_value.template_name);
    if (!tpl.has_value()) {
        return settings_value;
    }

    settings_value.width_text = tpl->width_text;
    settings_value.length_text = tpl->length_text;
    settings_value.response_text = tpl->response_text;
    return settings_value;
}

void active_edit_session::reset_active_template_settings() {
    active_template_settings_
        = normalized_template_settings(template_apply_settings {});
}

bool active_edit_session::has_template(const QString& template_name) const {
    return templates_.contains(active_edit_session_support::trimmed_name(template_name));
}

std::optional<active_edit_session::template_line>
active_edit_session::template_value(const QString& template_name) const {
    const QString trimmed_name
        = active_edit_session_support::trimmed_name(template_name);
    if (!templates_.contains(trimmed_name)) {
        return std::nullopt;
    }

    return templates_.value(trimmed_name);
}

stream_cell::line_instance active_edit_session::store_saved_line(
    const QString& stream_name, const QString& final_name,
    const std::vector<QPointF>& pts, const bool closed
) {
    stream_cell::line_instance inst;
    inst.template_name = active_edit_session_support::trimmed_name(final_name);
    inst.color = draft_line_profile_.color;
    inst.closed = closed;
    inst.width_text = draft_line_profile_.width_text;
    inst.length_text = draft_line_profile_.length_text;
    inst.response_text = draft_line_profile_.response_text;
    inst.pts_pct = pts;

    per_stream_lines_[stream_name].push_back(inst);
    templates_[inst.template_name] = template_line {
        pts,
        closed,
        draft_line_profile_.width_text,
        draft_line_profile_.length_text,
        draft_line_profile_.response_text,
    };

    return inst;
}

stream_cell::line_instance active_edit_session::store_applied_template_line(
    const QString& stream_name, const template_apply_settings& settings_value
) {
    const auto tpl = template_value(settings_value.template_name);
    if (!tpl.has_value()) {
        return {};
    }

    stream_cell::line_instance inst;
    inst.template_name
        = active_edit_session_support::trimmed_name(settings_value.template_name);
    inst.color = settings_value.color;
    inst.closed = tpl->closed;
    inst.width_text = settings_value.width_text;
    inst.length_text = settings_value.length_text;
    inst.response_text = settings_value.response_text;
    inst.pts_pct = tpl->pts_pct;

    per_stream_lines_[stream_name].push_back(inst);
    return inst;
}

const std::vector<stream_cell::line_instance>&
active_edit_session::stream_lines(const QString& stream_name) const {
    static const std::vector<stream_cell::line_instance> empty_lines;

    const auto it = per_stream_lines_.constFind(stream_name);
    return it == per_stream_lines_.cend() ? empty_lines : it.value();
}

QSet<QString> active_edit_session::used_template_names_for_stream(
    const QString& stream_name
) const {
    QSet<QString> used;

    for (const auto& inst : stream_lines(stream_name)) {
        const QString template_name = inst.template_name.trimmed();
        if (!template_name.isEmpty()) {
            used.insert(template_name);
        }
    }

    return used;
}

QStringList active_edit_session::template_candidates_excluding(
    const QSet<QString>& used
) const {
    QStringList candidates;
    candidates.reserve(templates_.size());

    for (auto it = templates_.cbegin(); it != templates_.cend(); ++it) {
        if (!used.contains(it.key())) {
            candidates.push_back(it.key());
        }
    }

    return candidates;
}

line_profile
active_edit_session::normalized_line_profile(line_profile profile_value) {
    profile_value.name
        = active_edit_session_support::trimmed_name(profile_value.name);
    profile_value.width_text = normalized_line_width_text(
        profile_value.width_text
    );
    profile_value.length_text = normalized_line_length_text(
        profile_value.length_text
    );
    profile_value.response_text = normalized_line_response_text(
        profile_value.response_text
    );
    return profile_value;
}

template_apply_settings active_edit_session::normalized_template_settings(
    template_apply_settings settings_value
) {
    settings_value.template_name
        = active_edit_session_support::trimmed_name(settings_value.template_name);
    settings_value.width_text = normalized_line_width_text(
        settings_value.width_text
    );
    settings_value.length_text = normalized_line_length_text(
        settings_value.length_text
    );
    settings_value.response_text = normalized_line_response_text(
        settings_value.response_text
    );
    return settings_value;
}
