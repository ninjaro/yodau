#ifndef YODAU_FRONTEND_WIDGETS_SETTINGS_PANEL_HPP
#define YODAU_FRONTEND_WIDGETS_SETTINGS_PANEL_HPP

#include "shell/frontend_log.hpp"
#include "shell/frontend_settings.hpp"

#include <QColor>
#include <QGroupBox>
#include <QSet>
#include <QStringList>
#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;
class algorithm_panel;
class line_profile_panel;
class template_apply_panel;

class settings_panel final : public QWidget {
    Q_OBJECT
public:
    explicit settings_panel(QWidget* parent = nullptr);

    // existing names
    void set_existing_names(QSet<QString> names);
    void add_existing_name(const QString& name);
    void remove_existing_name(const QString& name);

    void set_log_buffer(frontend_log_buffer* buffer);
    void set_log_mode(frontend_log_mode mode);
    frontend_log_mode log_mode() const;
    void append_log(frontend_log_entry entry) const;
    QString compose_current_log_report() const;
    QString compose_current_log_summary() const;
    bool write_current_log_report(const QString& path) const;

    // streams tab
    void add_stream_entry(
        const QString& name, const QString& source, bool checked = false
    ) const;
    void set_stream_checked(const QString& name, bool checked) const;
    void remove_stream_entry(const QString& name) const;
    void clear_stream_entries();

    void append_event(const QString& text) const;

    // add tab
    void set_local_sources(const QStringList& sources) const;
    void clear_add_inputs() const;
    void append_add_log(const QString& text) const;

    // active tab: streams
    void set_active_candidates(const QStringList& names) const;
    void set_active_current(const QString& name) const;
    void set_active_stream_settings(const stream_settings& settings_value);
    stream_settings current_active_stream_settings() const;

    // active tab: templates / lines
    void add_template_candidate(const QString& name) const;
    void set_template_candidates(const QStringList& names) const;
    void set_active_line_profile(const line_profile& profile);
    line_profile current_active_line_profile() const;
    void set_active_template_settings(
        const template_apply_settings& settings_value
    );
    template_apply_settings current_active_template_settings() const;

    void reset_active_line_form();
    void reset_active_template_form();

    void set_active_line_closed(bool closed) const;

    QString active_template_current() const;
    QColor active_template_preview_color() const;

    void append_active_log(const QString& msg) const;
    void clear_active_log() const;

signals:
    // add tab
    void add_file_stream(const QString& path, const QString& name, bool loop);
    void add_local_stream(const QString& source, const QString& name);
    void add_url_stream(const QString& url, const QString& name);
    void detect_local_sources_requested();

    // streams tab
    void show_stream_changed(const QString& name, bool show);

    // active tab
    void active_stream_settings_changed(stream_settings settings_value);

    void active_edit_mode_changed(bool drawing_new);

    void active_line_profile_changed(line_profile profile_value);
    void active_line_save_requested(line_profile profile_value);
    void active_line_undo_requested();

    void active_template_add_requested(
        template_apply_settings settings_value
    );
    void active_template_settings_changed(
        template_apply_settings settings_value
    );

private:
    enum class input_mode { file, local, url };

    // ui build
    void build_ui();
    QWidget* build_log_toolbar();
    QWidget* build_add_tab();
    QWidget* build_streams_tab();
    QWidget* build_active_tab();

    QWidget* build_active_stream_box(QWidget* parent);
    QWidget* build_edit_mode_box(QWidget* parent);
    QWidget* build_new_line_box(QWidget* parent);
    QWidget* build_templates_box(QWidget* parent);

    // add tab helpers
    void set_mode(input_mode mode);
    void update_add_tools() const;
    void update_add_enabled() const;

    void on_choose_file();
    void on_add_clicked();
    void on_refresh_local();
    void on_name_changed(QString text) const;

    QString resolved_name_for_current_input() const;
    bool name_is_unique(const QString& name) const;
    bool current_input_valid() const;

    void set_name_error(bool error) const;

    // active tab helpers
    void update_active_tools() const;
    void refresh_log_filter_options() const;
    bool entry_matches_log_filters(const frontend_log_entry& entry) const;
    frontend_log_area current_log_area() const;
    void rebuild_log_views() const;
    void rebuild_log_view(QPlainTextEdit* view, frontend_log_area area) const;

private slots:
    void on_log_mode_changed(int index);
    void on_log_severity_filter_changed(int index);
    void on_log_stream_filter_changed(int index);
    void on_log_subsystem_filter_changed(int index);
    void on_copy_logs_clicked();
    void on_copy_summary_clicked();
    void on_save_logs_clicked();
    void on_mode_group_clicked(int id);
    void on_local_source_changed();
    void on_url_text_changed();
    void on_active_combo_changed(const QString& text);
    void on_active_labels_toggled(bool checked);
    void on_algorithm_panel_settings_changed(stream_settings settings_value);
    void on_active_mode_clicked(int id);

    void on_stream_item_changed(QTreeWidgetItem* item, int column);

private:
    // common
    QTabWidget* tabs;
    QComboBox* log_mode_combo { nullptr };
    QComboBox* log_severity_filter_combo { nullptr };
    QComboBox* log_stream_filter_combo { nullptr };
    QComboBox* log_subsystem_filter_combo { nullptr };
    QPushButton* copy_logs_btn { nullptr };
    QPushButton* copy_summary_btn { nullptr };
    QPushButton* save_logs_btn { nullptr };
    frontend_log_mode current_log_mode { frontend_log_mode::release };
    int current_log_severity_filter { -1 };
    QString current_log_stream_filter;
    QString current_log_subsystem_filter;
    frontend_log_buffer* shared_log_buffer { nullptr };
    QSet<QString> existing_names;

    // add tab
    QWidget* add_tab;
    QLineEdit* name_edit;

    QButtonGroup* mode_group;
    QRadioButton* file_radio;
    QRadioButton* local_radio;
    QRadioButton* url_radio;
    input_mode current_mode;

    QGroupBox* add_file_box = nullptr;
    QLineEdit* file_path_edit;
    QPushButton* choose_file_btn;
    QCheckBox* loop_checkbox;

    QGroupBox* add_local_box = nullptr;
    QComboBox* local_sources_combo;
    QPushButton* refresh_local_btn;

    QGroupBox* add_url_box = nullptr;
    QLineEdit* url_edit;

    QPushButton* add_btn;
    QPlainTextEdit* add_log_view;

    // streams tab
    QWidget* streams_tab;
    QTreeWidget* streams_list;
    QPlainTextEdit* event_log_view;

    // active tab
    QWidget* active_tab { nullptr };
    QComboBox* active_combo { nullptr };
    QCheckBox* active_labels_cb = nullptr;
    algorithm_panel* active_algorithm_panel { nullptr };

    QGroupBox* active_mode_box = nullptr;
    QButtonGroup* active_mode_group { nullptr };
    QRadioButton* active_mode_draw_radio { nullptr };
    QRadioButton* active_mode_template_radio { nullptr };

    line_profile_panel* active_line_panel { nullptr };
    template_apply_panel* active_template_panel { nullptr };

    QPlainTextEdit* active_log_view = nullptr;
};

#endif // YODAU_FRONTEND_WIDGETS_SETTINGS_PANEL_HPP
