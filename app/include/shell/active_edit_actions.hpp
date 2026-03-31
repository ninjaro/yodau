#ifndef YODAU_FRONTEND_SHELL_ACTIVE_EDIT_ACTIONS_HPP
#define YODAU_FRONTEND_SHELL_ACTIVE_EDIT_ACTIONS_HPP

#include "shell/frontend_settings.hpp"
#include "widgets/stream_cell.hpp"

#include <QString>

namespace yodau::backend {
class stream_manager;
}

class active_edit_controller;
class active_edit_session;
class stream_widget_bridge;

class active_edit_actions final {
public:
    enum class line_save_status { saved, insufficient_points, backend_error };

    struct line_save_result {
        line_save_status status { line_save_status::insufficient_points };
        line_profile profile;
        int point_count { 0 };
        QString points_text;
        QString final_name;
        QString error_detail;
        stream_cell::line_instance line;
    };

    enum class template_apply_status {
        applied,
        unknown_template,
        backend_error,
    };

    struct template_apply_result {
        template_apply_status status { template_apply_status::unknown_template };
        template_apply_settings settings;
        QString error_detail;
        stream_cell::line_instance line;
    };

    active_edit_actions(
        yodau::backend::stream_manager* stream_mgr,
        active_edit_session& edit_session, stream_widget_bridge& widget_bridge,
        active_edit_controller& edit_controller
    );

    [[nodiscard]] line_save_result save_active_line(
        const QString& active_name, line_profile profile_value,
        stream_cell& cell
    ) const;

    [[nodiscard]] template_apply_result apply_active_template(
        const QString& active_name, template_apply_settings settings_value,
        stream_cell& cell
    ) const;

private:
    static QString points_str_from_pct(const std::vector<QPointF>& pts);

    yodau::backend::stream_manager* stream_mgr_ { nullptr };
    active_edit_session& edit_session_;
    stream_widget_bridge& widget_bridge_;
    active_edit_controller& edit_controller_;
};

#endif // YODAU_FRONTEND_SHELL_ACTIVE_EDIT_ACTIONS_HPP
