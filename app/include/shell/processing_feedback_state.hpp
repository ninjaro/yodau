#ifndef YODAU_APP_SHELL_PROCESSING_FEEDBACK_STATE_HPP
#define YODAU_APP_SHELL_PROCESSING_FEEDBACK_STATE_HPP

#include "core/namespace_alias.hpp"
#include "shell/app_log.hpp"
#include "streams/event.hpp"

#include <QColor>
#include <QDateTime>
#include <QHash>
#include <QPointF>
#include <QString>

#include <chrono>
#include <deque>
#include <optional>

class processing_feedback_state final {
public:
    struct tripwire_visual_feedback {
        QString direction;
        double strength { 1.0 };
        double speed { 1.0 };
    };

    struct processed_event {
        QString stream_name;
        QString line_name;
        QString kind_text;
        QString log_message;
        QString log_detail;
        app_log_severity log_severity { app_log_severity::info };
        std::optional<QPointF> overlay_position_pct;
        QColor overlay_color;
        std::optional<tripwire_visual_feedback> tripwire_visual;
        bool allow_gui_overlay { true };
        bool motion_activity_changed { false };
    };

    processed_event consume_event(
        const yodau::core::event& event_value,
        const QDateTime& current_time = QDateTime::currentDateTime()
    );

    int recent_motion_count();

    static QString event_kind_text(yodau::core::event_kind kind);

private:
    QHash<QString, QDateTime> last_gui_motion_event_ts_;
    int motion_gui_interval_ms_ { 80 };
    std::deque<std::chrono::steady_clock::time_point> recent_motion_events_;
};

#endif // YODAU_APP_SHELL_PROCESSING_FEEDBACK_STATE_HPP
