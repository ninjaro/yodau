#ifndef YODAU_FRONTEND_WIDGETS_LOG_TOOLBAR_PANEL_HPP
#define YODAU_FRONTEND_WIDGETS_LOG_TOOLBAR_PANEL_HPP

#include "shell/frontend_log.hpp"

#include <QStringList>
#include <QWidget>
#include <optional>

class QComboBox;
class QLineEdit;
class QPushButton;

class log_toolbar_panel final : public QWidget {
    Q_OBJECT

public:
    explicit log_toolbar_panel(QWidget* parent = nullptr);

    void set_log_buffer(frontend_log_buffer* buffer);
    bool append_entry(frontend_log_entry entry) const;

    void set_log_mode(frontend_log_mode mode);
    frontend_log_mode log_mode() const;
    bool entry_matches(const frontend_log_entry& entry) const;

    QVector<frontend_log_entry> filtered_entries(
        std::optional<frontend_log_area> area = std::nullopt
    ) const;
    QStringList formatted_entries(
        std::optional<frontend_log_area> area = std::nullopt
    ) const;
    QString compose_log_report(
        std::optional<frontend_log_area> area = std::nullopt
    ) const;
    QString compose_log_summary(
        std::optional<frontend_log_area> area = std::nullopt
    ) const;
    bool write_log_report(
        std::optional<frontend_log_area> area, const QString& path
    ) const;

signals:
    void view_state_changed();
    void log_mode_changed(frontend_log_mode mode);
    void copy_logs_requested();
    void copy_summary_requested();
    void save_logs_requested();

private slots:
    void on_log_mode_changed(int index);
    void on_log_area_filter_changed(int index);
    void on_log_severity_filter_changed(int index);
    void on_log_event_filter_changed(int index);
    void on_log_stream_filter_changed(int index);
    void on_log_subsystem_filter_changed(int index);
    void on_log_search_filter_changed(const QString& text);

private:
    void build_ui();
    void refresh_filter_options() const;

    QComboBox* log_mode_combo { nullptr };
    QComboBox* log_area_filter_combo { nullptr };
    QComboBox* log_severity_filter_combo { nullptr };
    QComboBox* log_event_filter_combo { nullptr };
    QComboBox* log_stream_filter_combo { nullptr };
    QComboBox* log_subsystem_filter_combo { nullptr };
    QLineEdit* log_search_filter_edit { nullptr };
    QPushButton* copy_logs_btn { nullptr };
    QPushButton* copy_summary_btn { nullptr };
    QPushButton* save_logs_btn { nullptr };

    frontend_log_mode current_log_mode { frontend_log_mode::release };
    int current_log_area_filter { -1 };
    int current_log_severity_filter { -1 };
    QString current_log_event_filter;
    QString current_log_stream_filter;
    QString current_log_subsystem_filter;
    QString current_log_search_filter;
    frontend_log_buffer* shared_log_buffer { nullptr };
};

#endif // YODAU_FRONTEND_WIDGETS_LOG_TOOLBAR_PANEL_HPP
