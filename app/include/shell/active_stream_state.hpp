#ifndef YODAU_FRONTEND_SHELL_ACTIVE_STREAM_STATE_HPP
#define YODAU_FRONTEND_SHELL_ACTIVE_STREAM_STATE_HPP

#include "shell/frontend_settings.hpp"

#include <QString>

class active_edit_session;
class stream_catalog_state;
class stream_route_state;
class stream_widget_bridge;

class active_stream_state final {
public:
    struct selection_result {
        QString active_name;
        stream_settings settings;
        bool changed { false };
    };

    struct settings_result {
        enum class outcome {
            switched_active_stream,
            ignored_empty_stream,
            updated,
        };

        outcome outcome_value { outcome::ignored_empty_stream };
        QString active_name;
        QString previous_active_name;
        stream_settings settings;
        stream_settings previous_settings;
        bool labels_changed { false };
        bool algorithm_changed { false };
    };

    active_stream_state(
        stream_catalog_state& catalog_state, stream_route_state& route_state,
        stream_widget_bridge& widget_bridge, active_edit_session& edit_session
    );

    [[nodiscard]] selection_result set_active_stream(const QString& name) const;
    [[nodiscard]] settings_result apply_stream_settings(
        stream_settings settings_value
    ) const;

private:
    stream_catalog_state& catalog_state_;
    stream_route_state& route_state_;
    stream_widget_bridge& widget_bridge_;
    active_edit_session& edit_session_;
};

#endif // YODAU_FRONTEND_SHELL_ACTIVE_STREAM_STATE_HPP
