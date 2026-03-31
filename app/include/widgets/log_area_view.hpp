#ifndef YODAU_FRONTEND_WIDGETS_LOG_AREA_VIEW_HPP
#define YODAU_FRONTEND_WIDGETS_LOG_AREA_VIEW_HPP

#include "shell/frontend_log.hpp"

#include <QPlainTextEdit>

class log_toolbar_panel;

class log_area_view final : public QPlainTextEdit {
public:
    explicit log_area_view(
        frontend_log_area area, QWidget* parent = nullptr
    );

    void set_log_toolbar(log_toolbar_panel* toolbar);
    bool append_entry(const frontend_log_entry& entry) const;

private:
    void rebuild_from_toolbar() const;

    frontend_log_area log_area;
    log_toolbar_panel* log_toolbar_widget { nullptr };
};

#endif // YODAU_FRONTEND_WIDGETS_LOG_AREA_VIEW_HPP
