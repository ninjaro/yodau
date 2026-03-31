#include "shell/active_edit_actions.hpp"

#include "shell/active_edit_controller.hpp"
#include "shell/active_edit_session.hpp"
#include "shell/stream_widget_bridge.hpp"
#include "streams/stream_manager.hpp"

#include <algorithm>
#include <QStringList>
#include <utility>

namespace active_edit_actions_support {

QString backend_unavailable_detail() {
    return QStringLiteral("backend stream manager unavailable");
}

float effective_length_value(const QString& length_text) {
    const QString normalized = normalized_line_length_text(length_text);
    if (normalized == QStringLiteral("short")) {
        return 0.75f;
    }
    if (normalized == QStringLiteral("long")) {
        return 1.35f;
    }

    bool ok = false;
    const float numeric_value = normalized.toFloat(&ok);
    if (ok && numeric_value > 0.0f) {
        return std::clamp(numeric_value, 0.25f, 4.0f);
    }

    return 1.0f;
}

float damping_value(const QString& response_text) {
    const QString normalized = normalized_line_response_text(response_text);
    if (normalized == QStringLiteral("dry")) {
        return 0.25f;
    }
    if (normalized == QStringLiteral("resonant")) {
        return 0.8f;
    }

    bool ok = false;
    const float numeric_value = normalized.toFloat(&ok);
    if (ok && numeric_value >= 0.0f) {
        return std::clamp(numeric_value, 0.0f, 1.0f);
    }

    return 0.5f;
}

float interaction_width_value(
    const QString& width_text, const float visual_width
) {
    const QString normalized = normalized_line_width_text(width_text);
    if (normalized == QStringLiteral("string_light")) {
        return visual_width * 1.2f;
    }
    if (normalized == QStringLiteral("string_heavy")) {
        return visual_width * 1.3f;
    }
    return visual_width;
}

yodau::backend::line_profile backend_profile_from_line_editor(
    const QString& line_name, const line_profile& profile_value
) {
    const float visual_width
        = static_cast<float>(line_width_visual_value(profile_value.width_text));
    return yodau::backend::make_line_profile(
        line_name.toStdString(), visual_width,
        interaction_width_value(profile_value.width_text, visual_width),
        effective_length_value(profile_value.length_text),
        damping_value(profile_value.response_text)
    );
}

yodau::backend::line_profile backend_profile_from_template_settings(
    const QString& line_name, const template_apply_settings& settings_value
) {
    const float visual_width
        = static_cast<float>(line_width_visual_value(settings_value.width_text));
    return yodau::backend::make_line_profile(
        line_name.toStdString(), visual_width,
        interaction_width_value(settings_value.width_text, visual_width),
        effective_length_value(settings_value.length_text),
        damping_value(settings_value.response_text)
    );
}

} // namespace active_edit_actions_support

active_edit_actions::active_edit_actions(
    yodau::backend::stream_manager* stream_mgr,
    active_edit_session& edit_session, stream_widget_bridge& widget_bridge,
    active_edit_controller& edit_controller
)
    : stream_mgr_(stream_mgr)
    , edit_session_(edit_session)
    , widget_bridge_(widget_bridge)
    , edit_controller_(edit_controller) {}

active_edit_actions::line_save_result active_edit_actions::save_active_line(
    const QString& active_name, line_profile profile_value, stream_cell& cell
) const {
    line_save_result result;

    edit_session_.set_draft_line_profile(std::move(profile_value));
    result.profile = edit_session_.draft_line_profile();

    const auto pts = cell.draft_points_pct();
    result.point_count = static_cast<int>(pts.size());
    if (pts.size() < 2) {
        return result;
    }

    result.points_text = points_str_from_pct(pts);

    if (stream_mgr_ == nullptr) {
        result.status = line_save_status::backend_error;
        result.error_detail = active_edit_actions_support::backend_unavailable_detail();
        return result;
    }

    try {
        const auto line_ptr = stream_mgr_->add_line(
            result.points_text.toStdString(), result.profile.closed,
            result.profile.name.toStdString()
        );

        result.final_name = QString::fromStdString(line_ptr->name);
        stream_mgr_->set_line_profile(
            active_edit_actions_support::backend_profile_from_line_editor(
                result.final_name, result.profile
            )
        );
        stream_mgr_->set_line(
            active_name.toStdString(), result.final_name.toStdString()
        );

        result.line = edit_session_.store_saved_line(
            active_name, result.final_name, pts, result.profile.closed
        );
        cell.add_persistent_line(result.line);

        edit_controller_.reset_after_line_saved(result.final_name);
        widget_bridge_.sync_active_persistent(active_name, edit_session_);

        result.status = line_save_status::saved;
        return result;
    } catch (const std::exception& e) {
        result.status = line_save_status::backend_error;
        result.error_detail = QString::fromLocal8Bit(e.what());
        return result;
    }
}

active_edit_actions::template_apply_result
active_edit_actions::apply_active_template(
    const QString& active_name, template_apply_settings settings_value,
    stream_cell& cell
) const {
    template_apply_result result;

    edit_session_.set_active_template_settings(std::move(settings_value));
    result.settings = edit_session_.active_template_settings();

    if (!edit_session_.has_template(result.settings.template_name)) {
        return result;
    }

    if (stream_mgr_ == nullptr) {
        result.status = template_apply_status::backend_error;
        result.error_detail = active_edit_actions_support::backend_unavailable_detail();
        return result;
    }

    try {
        stream_mgr_->set_line(
            active_name.toStdString(),
            result.settings.template_name.toStdString()
        );
        stream_mgr_->set_stream_line_profile(
            active_name.toStdString(),
            active_edit_actions_support::backend_profile_from_template_settings(
                result.settings.template_name, result.settings
            )
        );
    } catch (const std::exception& e) {
        result.status = template_apply_status::backend_error;
        result.error_detail = QString::fromLocal8Bit(e.what());
        return result;
    }

    result.line = edit_session_.store_applied_template_line(
        active_name, result.settings
    );
    cell.add_persistent_line(result.line);

    edit_controller_.reset_after_template_applied();
    widget_bridge_.sync_active_persistent(active_name, edit_session_);

    result.status = template_apply_status::applied;
    return result;
}

QString active_edit_actions::points_str_from_pct(
    const std::vector<QPointF>& pts
) {
    QStringList parts;
    parts.reserve(static_cast<int>(pts.size()));
    for (const auto& p : pts) {
        parts << QString("(%1,%2)").arg(p.x(), 0, 'f', 3).arg(p.y(), 0, 'f', 3);
    }
    return parts.join("; ");
}
