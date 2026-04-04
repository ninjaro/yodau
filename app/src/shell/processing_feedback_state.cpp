#include "shell/processing_feedback_state.hpp"
#include "core/namespace_alias.hpp"

namespace processing_feedback_state_support {

using steady_clock = std::chrono::steady_clock;

constexpr auto motion_activity_window = std::chrono::seconds(3);

struct tripwire_visual_payload {
    QString direction;
    double strength { 1.0 };
    double speed { 1.0 };
};

tripwire_visual_payload
parse_tripwire_visual_payload(const std::string& message) {
    tripwire_visual_payload payload;
    if (message.empty()) {
        return payload;
    }

    const auto parts = QString::fromStdString(message).split('|');
    if (!parts.isEmpty()) {
        payload.direction = parts[0].trimmed();
    }

    if (parts.size() >= 2) {
        bool ok = false;
        const double value = parts[1].toDouble(&ok);
        if (ok && value > 0.0) {
            payload.strength = value;
        }
    }

    if (parts.size() >= 3) {
        bool ok = false;
        const double value = parts[2].toDouble(&ok);
        if (ok && value > 0.0) {
            payload.speed = value;
        }
    } else {
        payload.speed = payload.strength;
    }

    return payload;
}

void prune_expired_motion_events(
    std::deque<steady_clock::time_point>& motion_events,
    const steady_clock::time_point now
) {
    while (!motion_events.empty()
           && now - motion_events.front() > motion_activity_window) {
        motion_events.pop_front();
    }
}

QColor overlay_color_for_event_kind(const yodau::core::event_kind kind) {
    switch (kind) {
    case yodau::core::event_kind::motion:
        return QColor(QStringLiteral("#f4a261"));
    case yodau::core::event_kind::tripwire:
        return QColor(QStringLiteral("#e63946"));
    case yodau::core::event_kind::roi:
        return QColor(QStringLiteral("#2a9d8f"));
    case yodau::core::event_kind::info:
    default:
        return QColor(QStringLiteral("#adb5bd"));
    }
}

QString event_detail_text(const yodau::core::event& event_value) {
    const QString line_name = QString::fromStdString(event_value.line_name);
    QString event_detail;

    if (!line_name.isEmpty()) {
        event_detail = QStringLiteral("line=%1").arg(line_name);
    }

    if (event_value.pos_pct.has_value()) {
        const auto& position = *event_value.pos_pct;
        const auto position_text = QStringLiteral("pos=(%1,%2)")
                                       .arg(position.x, 0, 'f', 3)
                                       .arg(position.y, 0, 'f', 3);
        if (!event_detail.isEmpty()) {
            event_detail.append(' ');
        }
        event_detail.append(position_text);
    }

    const auto core_message
        = QString::fromStdString(event_value.message).trimmed();
    if (!core_message.isEmpty()) {
        if (!event_detail.isEmpty()) {
            event_detail.append(' ');
        }
        event_detail.append(QStringLiteral("core=%1").arg(core_message));
    }

    return event_detail;
}

} // namespace processing_feedback_state_support

processing_feedback_state::processed_event
processing_feedback_state::consume_event(
    const yodau::core::event& event_value, const QDateTime& current_time
) {
    processed_event result;
    result.stream_name = QString::fromStdString(event_value.stream_name);
    result.line_name = QString::fromStdString(event_value.line_name);
    result.kind_text = event_kind_text(event_value.kind);
    result.log_detail
        = processing_feedback_state_support::event_detail_text(event_value);
    result.overlay_color
        = processing_feedback_state_support::overlay_color_for_event_kind(
            event_value.kind
        );

    if (event_value.pos_pct.has_value()) {
        const auto& position = *event_value.pos_pct;
        result.overlay_position_pct = QPointF(position.x, position.y);
    }

    switch (event_value.kind) {
    case yodau::core::event_kind::motion:
        result.log_message = QStringLiteral("motion detected");
        result.log_severity = app_log_severity::debug;
        break;
    case yodau::core::event_kind::tripwire:
        result.log_message = QStringLiteral("tripwire triggered");
        break;
    case yodau::core::event_kind::roi:
        result.log_message = QStringLiteral("roi event");
        break;
    case yodau::core::event_kind::info:
    default:
        result.log_message = QStringLiteral("core info event");
        break;
    }

    if (event_value.kind == yodau::core::event_kind::tripwire
        && result.overlay_position_pct.has_value() && !result.line_name.isEmpty()) {
        const auto payload
            = processing_feedback_state_support::parse_tripwire_visual_payload(
                event_value.message
            );
        result.tripwire_visual = tripwire_visual_feedback {
            .direction = payload.direction,
            .strength = payload.strength,
            .speed = payload.speed,
        };
    }

    if (event_value.kind != yodau::core::event_kind::motion) {
        return result;
    }

    result.motion_activity_changed = true;
    const auto now = processing_feedback_state_support::steady_clock::now();
    recent_motion_events_.push_back(now);
    processing_feedback_state_support::prune_expired_motion_events(
        recent_motion_events_, now
    );

    if (!result.stream_name.isEmpty()
        && last_gui_motion_event_ts_.contains(result.stream_name)) {
        const int age = static_cast<int>(
            last_gui_motion_event_ts_[result.stream_name].msecsTo(current_time)
        );
        if (age < motion_gui_interval_ms_) {
            result.allow_gui_overlay = false;
            return result;
        }
    }

    if (!result.stream_name.isEmpty()) {
        last_gui_motion_event_ts_[result.stream_name] = current_time;
    }

    return result;
}

int processing_feedback_state::recent_motion_count() {
    const auto now = processing_feedback_state_support::steady_clock::now();
    processing_feedback_state_support::prune_expired_motion_events(
        recent_motion_events_, now
    );
    return static_cast<int>(recent_motion_events_.size());
}

QString
processing_feedback_state::event_kind_text(yodau::core::event_kind kind) {
    switch (kind) {
    case yodau::core::event_kind::motion:
        return QStringLiteral("motion");
    case yodau::core::event_kind::tripwire:
        return QStringLiteral("tripwire");
    case yodau::core::event_kind::roi:
        return QStringLiteral("roi");
    case yodau::core::event_kind::info:
    default:
        return QStringLiteral("info");
    }
}
