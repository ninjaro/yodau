#include "shell/active_edit_actions.hpp"

#include "core/namespace_alias.hpp"
#include "shell/active_edit_controller.hpp"
#include "shell/active_edit_session.hpp"
#include "shell/stream_widget_bridge.hpp"
#include "streams/stream_manager.hpp"

#include <QStringList>
#include <algorithm>
#include <utility>

namespace active_edit_actions_support {

QString core_unavailable_detail() {
    return QStringLiteral("core stream manager unavailable");
}

line_edit_request normalized_line_edit_request(line_edit_request request) {
    request.stream_name = request.stream_name.trimmed();
    request.source_line_name = request.source_line_name.trimmed();
    request.profile.name = request.profile.name.trimmed();
    request.profile.color_mode_id
        = normalized_line_color_mode_id(request.profile.color_mode_id);
    request.profile.width_text
        = normalized_line_width_text(request.profile.width_text);
    request.profile.length_text
        = normalized_line_length_text(request.profile.length_text);
    request.profile.response_text
        = normalized_line_response_text(request.profile.response_text);
    if (request.selected_visible_index
        >= static_cast<int>(request.points_pct.size())) {
        request.selected_visible_index = -1;
    }
    return request;
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

yodau::core::line_profile core_profile_from_line_editor(
    const QString& line_name, const line_profile& profile_value
) {
    const auto visual_width
        = static_cast<float>(line_width_visual_value(profile_value.width_text));
    return yodau::core::make_line_profile(
        line_name.toStdString(), visual_width,
        interaction_width_value(profile_value.width_text, visual_width),
        effective_length_value(profile_value.length_text),
        damping_value(profile_value.response_text)
    );
}

yodau::core::line_profile core_profile_from_template_settings(
    const QString& line_name, const template_apply_settings& settings_value
) {
    const auto visual_width = static_cast<float>(
        line_width_visual_value(settings_value.width_text)
    );
    return yodau::core::make_line_profile(
        line_name.toStdString(), visual_width,
        interaction_width_value(settings_value.width_text, visual_width),
        effective_length_value(settings_value.length_text),
        damping_value(settings_value.response_text)
    );
}

yodau::core::line_profile
core_profile_from_line_instance(const stream_cell::line_instance& line_value) {
    const auto visual_width
        = static_cast<float>(line_width_visual_value(line_value.width_text));
    return yodau::core::make_line_profile(
        line_value.template_name.toStdString(), visual_width,
        interaction_width_value(line_value.width_text, visual_width),
        effective_length_value(line_value.length_text),
        damping_value(line_value.response_text)
    );
}

std::vector<yodau::core::point>
core_points_from_pct(const std::vector<QPointF>& points) {
    std::vector<yodau::core::point> converted;
    converted.reserve(points.size());
    for (const QPointF& point_value : points) {
        converted.push_back(yodau::core::point {
            .x = static_cast<float>(point_value.x()),
            .y = static_cast<float>(point_value.y()),
        });
    }
    return converted;
}

} // namespace active_edit_actions_support

active_edit_actions::active_edit_actions(
    yodau::core::stream_manager* stream_mgr, active_edit_session& edit_session,
    stream_widget_bridge& widget_bridge, active_edit_controller& edit_controller
)
    : stream_mgr_(stream_mgr)
    , edit_session_(edit_session)
    , widget_bridge_(widget_bridge)
    , edit_controller_(edit_controller) { }

active_edit_actions::line_save_result active_edit_actions::save_active_line(
    const QString& active_name, line_profile profile_value, stream_cell& cell
) const {
    line_save_result result;

    edit_session_.set_draft_line_profile(std::move(profile_value));
    result.profile = edit_session_.draft_line_profile();

    const auto pts = cell.draft_points_pct();
    result.point_count = static_cast<int>(pts.size());
    const size_t minimum_points = result.profile.closed ? 3U : 2U;
    if (pts.size() < minimum_points) {
        return result;
    }

    result.points_text = points_str_from_pct(pts);

    if (stream_mgr_ == nullptr) {
        result.status = line_save_status::core_error;
        result.error_detail
            = active_edit_actions_support::core_unavailable_detail();
        return result;
    }

    try {
        const auto line_ptr = stream_mgr_->add_line(
            active_edit_actions_support::core_points_from_pct(pts),
            result.profile.closed,
            result.profile.name.toStdString()
        );

        result.final_name = QString::fromStdString(line_ptr->name);
        stream_mgr_->set_line_profile(
            active_edit_actions_support::core_profile_from_line_editor(
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
        result.status = line_save_status::core_error;
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
        result.status = template_apply_status::core_error;
        result.error_detail
            = active_edit_actions_support::core_unavailable_detail();
        return result;
    }

    try {
        stream_mgr_->set_line(
            active_name.toStdString(),
            result.settings.template_name.toStdString()
        );
        stream_mgr_->set_stream_line_profile(
            active_name.toStdString(),
            active_edit_actions_support::core_profile_from_template_settings(
                result.settings.template_name, result.settings
            )
        );
    } catch (const std::exception& e) {
        result.status = template_apply_status::core_error;
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

active_edit_actions::line_toggle_result
active_edit_actions::set_stream_line_enabled(
    const QString& stream_name, const QString& line_name, const bool enabled
) const {
    line_toggle_result result;
    result.stream_name = stream_name.trimmed();
    result.line_name = line_name.trimmed();
    result.enabled = enabled;

    const auto line_value
        = edit_session_.find_stream_line(result.stream_name, result.line_name);
    if (!line_value.has_value()) {
        return result;
    }

    result.line = *line_value;

    if (stream_mgr_ == nullptr) {
        result.status = line_toggle_status::core_error;
        result.error_detail
            = active_edit_actions_support::core_unavailable_detail();
        return result;
    }

    try {
        if (enabled) {
            stream_mgr_->set_line(
                result.stream_name.toStdString(), result.line_name.toStdString()
            );
            stream_mgr_->set_stream_line_profile(
                result.stream_name.toStdString(),
                active_edit_actions_support::core_profile_from_line_instance(
                    result.line
                )
            );
        } else {
            stream_mgr_->clear_stream_line(
                result.stream_name.toStdString(), result.line_name.toStdString()
            );
        }
    } catch (const std::exception& e) {
        result.status = line_toggle_status::core_error;
        result.error_detail = QString::fromLocal8Bit(e.what());
        return result;
    }

    if (!edit_session_.set_stream_line_enabled(
            result.stream_name, result.line_name, enabled
        )) {
        result.status = line_toggle_status::missing_line;
        return result;
    }

    result.line.enabled = enabled;
    widget_bridge_.sync_active_persistent(result.stream_name, edit_session_);
    result.status = line_toggle_status::updated;
    return result;
}

active_edit_actions::line_detach_result active_edit_actions::detach_stream_line(
    const QString& stream_name, const QString& line_name
) const {
    line_detach_result result;
    result.stream_name = stream_name.trimmed();
    result.line_name = line_name.trimmed();

    const auto line_value
        = edit_session_.find_stream_line(result.stream_name, result.line_name);
    if (!line_value.has_value()) {
        return result;
    }

    result.line = *line_value;

    if (stream_mgr_ == nullptr) {
        result.status = line_detach_status::core_error;
        result.error_detail
            = active_edit_actions_support::core_unavailable_detail();
        return result;
    }

    try {
        stream_mgr_->clear_stream_line(
            result.stream_name.toStdString(), result.line_name.toStdString()
        );
    } catch (const std::exception& e) {
        result.status = line_detach_status::core_error;
        result.error_detail = QString::fromLocal8Bit(e.what());
        return result;
    }

    if (!edit_session_.detach_stream_line(
            result.stream_name, result.line_name
        )) {
        result.status = line_detach_status::missing_line;
        return result;
    }

    widget_bridge_.sync_active_persistent(result.stream_name, edit_session_);
    result.status = line_detach_status::detached;
    return result;
}

active_edit_actions::line_edit_save_result
active_edit_actions::save_active_line_edit(
    const QString& active_name, line_edit_request request
) const {
    line_edit_save_result result;
    result.request = active_edit_actions_support::normalized_line_edit_request(
        std::move(request)
    );
    result.request.stream_name = active_name.trimmed();
    result.point_count = static_cast<int>(result.request.points_pct.size());
    if (result.request.profile.name.isEmpty()) {
        result.status = line_edit_save_status::missing_name;
        return result;
    }

    const qsizetype minimum_points = result.request.profile.closed ? 3 : 2;
    if (result.request.points_pct.size() < minimum_points) {
        result.status = line_edit_save_status::insufficient_points;
        return result;
    }

    std::vector<QPointF> points;
    points.reserve(
        static_cast<std::vector<QPointF>::size_type>(
            result.request.points_pct.size()
        )
    );
    for (const QPointF& point_value : result.request.points_pct) {
        points.push_back(point_value);
    }
    result.points_text = points_str_from_pct(points);

    const auto source_line = edit_session_.find_stream_line(
        result.request.stream_name, result.request.source_line_name
    );
    if (!source_line.has_value()) {
        result.status = line_edit_save_status::missing_source_line;
        return result;
    }

    result.source_line = *source_line;

    if (stream_mgr_ == nullptr) {
        result.status = line_edit_save_status::core_error;
        result.error_detail
            = active_edit_actions_support::core_unavailable_detail();
        return result;
    }

    try {
        const auto line_ptr = stream_mgr_->add_line(
            active_edit_actions_support::core_points_from_pct(points),
            result.request.profile.closed,
            result.request.profile.name.toStdString()
        );

        result.final_name = QString::fromStdString(line_ptr->name);
        stream_mgr_->set_line_profile(
            active_edit_actions_support::core_profile_from_line_editor(
                result.final_name, result.request.profile
            )
        );
        stream_mgr_->set_line(
            result.request.stream_name.toStdString(),
            result.final_name.toStdString()
        );
        stream_mgr_->clear_stream_line(
            result.request.stream_name.toStdString(),
            result.request.source_line_name.toStdString()
        );

        edit_session_.detach_stream_line(
            result.request.stream_name, result.request.source_line_name
        );

        stream_cell::line_instance line_value = result.source_line;
        line_value.template_name = result.final_name;
        line_value.color = result.request.profile.color;
        line_value.color_mode_id = result.request.profile.color_mode_id;
        line_value.closed = result.request.profile.closed;
        line_value.width_text = result.request.profile.width_text;
        line_value.length_text = result.request.profile.length_text;
        line_value.response_text = result.request.profile.response_text;
        line_value.enabled = true;
        line_value.pts_pct = std::move(points);

        result.line = edit_session_.store_stream_line(
            result.request.stream_name, std::move(line_value)
        );
        edit_controller_.reset_after_line_edit_saved();
        widget_bridge_.sync_active_persistent(
            result.request.stream_name, edit_session_
        );

        result.status = line_edit_save_status::saved;
        return result;
    } catch (const std::exception& e) {
        result.status = line_edit_save_status::core_error;
        result.error_detail = QString::fromLocal8Bit(e.what());
        return result;
    }
}

QString
active_edit_actions::points_str_from_pct(const std::vector<QPointF>& pts) {
    QStringList parts;
    parts.reserve(static_cast<int>(pts.size()));
    for (const auto& p : pts) {
        parts << QString("(%1,%2)").arg(p.x(), 0, 'f', 3).arg(p.y(), 0, 'f', 3);
    }
    return parts.join("; ");
}
