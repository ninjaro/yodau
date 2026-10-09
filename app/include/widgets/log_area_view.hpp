#ifndef YODAU_APP_WIDGETS_LOG_AREA_VIEW_HPP
#define YODAU_APP_WIDGETS_LOG_AREA_VIEW_HPP

#include "shell/app_log.hpp"

#include <QPlainTextEdit>
#include <QPointer>
#include <optional>

class log_toolbar_panel;
class QSyntaxHighlighter;

class log_area_view final : public QPlainTextEdit {
public:
    explicit log_area_view(
        std::optional<app_log_area> area = std::nullopt,
        QWidget* parent = nullptr
    );

    void set_log_toolbar(log_toolbar_panel* toolbar);
    bool append_entry(const app_log_entry& entry) const;
    const QVector<app_log_entry>& visible_entries() const;
    /** Formatting of displayed entries, including a frozen view. */
    app_log_mode active_mode() const;

protected:
    void changeEvent(QEvent* event) override;

private:
    void rebuild_from_toolbar() const;
    void set_visible_entries(const QVector<app_log_entry>& entries) const;
    void apply_scroll(int position) const;

    std::optional<app_log_area> log_area;
    QPointer<log_toolbar_panel> log_toolbar_widget;
    QSyntaxHighlighter* log_highlighter_ { nullptr };
    mutable QVector<app_log_entry> visible_entries_;
    mutable app_log_mode display_mode_ { app_log_mode::release };
    mutable bool refresh_pending_ { false };
};

#endif // YODAU_APP_WIDGETS_LOG_AREA_VIEW_HPP
