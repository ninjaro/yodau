#ifndef YODAU_FRONTEND_WIDGETS_LOG_AREA_VIEW_HPP
#define YODAU_FRONTEND_WIDGETS_LOG_AREA_VIEW_HPP

#include "shell/frontend_log.hpp"

#include <QPlainTextEdit>
#include <optional>

class log_toolbar_panel;
class QSyntaxHighlighter;

class log_area_view final : public QPlainTextEdit {
public:
    explicit log_area_view(
        std::optional<frontend_log_area> area = std::nullopt,
        QWidget* parent = nullptr
    );

    void set_log_toolbar(log_toolbar_panel* toolbar);
    bool append_entry(const frontend_log_entry& entry) const;
    const QVector<frontend_log_entry>& visible_entries() const;
    frontend_log_mode active_mode() const;

private:
    void rebuild_from_toolbar() const;
    void set_visible_entries(const QVector<frontend_log_entry>& entries) const;

    std::optional<frontend_log_area> log_area;
    log_toolbar_panel* log_toolbar_widget { nullptr };
    QSyntaxHighlighter* log_highlighter_ { nullptr };
    mutable QVector<frontend_log_entry> visible_entries_;
};

#endif // YODAU_FRONTEND_WIDGETS_LOG_AREA_VIEW_HPP
