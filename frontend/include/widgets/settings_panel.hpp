#ifndef YODAU_FRONTEND_WIDGETS_SETTINGS_PANEL_HPP
#define YODAU_FRONTEND_WIDGETS_SETTINGS_PANEL_HPP

#include <QColor>
#include <QGroupBox>
#include <QSet>
#include <QStringList>
#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

class settings_panel final : public QWidget {
    Q_OBJECT
public:
    explicit settings_panel(QWidget* parent = nullptr);

    // existing names
    void set_existing_names(QSet<QString> names);
    void add_existing_name(const QString& name);
    void remove_existing_name(const QString& name);

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

    // active tab: templates / lines
    void add_template_candidate(const QString& name) const;
    void set_template_candidates(const QStringList& names) const;

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
    void active_stream_selected(const QString& name);

    void active_edit_mode_changed(bool drawing_new);

    void active_line_params_changed(
        const QString& name, const QColor& color, bool closed
    );
    void active_line_save_requested(const QString& name, bool closed);
    void active_line_undo_requested();
    void active_labels_enabled_changed(bool on);

    void active_template_selected(const QString& template_name);
    void active_template_color_changed(const QColor& color);
    void active_template_add_requested(
        const QString& template_name, const QColor& color
    );

private:
    enum class input_mode { file, local, url };

    // ui build
    void build_ui();
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
    void set_btn_color(QPushButton* btn, const QColor& c) const;

private slots:
    void on_active_combo_changed(const QString& text);
    void on_active_mode_clicked(int id);

    void on_active_line_color_clicked();
    void on_active_line_undo_clicked();
    void on_active_line_save_clicked();
    void on_active_line_name_finished();
    void on_active_line_closed_toggled(bool checked);

    void on_active_template_combo_changed(const QString& text);
    void on_active_template_color_clicked();
    void on_active_template_add_clicked();

    void on_stream_item_changed(QTreeWidgetItem* item, int column);

private:
    // common
    QTabWidget* tabs;
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

    QGroupBox* active_mode_box = nullptr;
    QButtonGroup* active_mode_group { nullptr };
    QRadioButton* active_mode_draw_radio { nullptr };
    QRadioButton* active_mode_template_radio { nullptr };

    QGroupBox* active_line_box { nullptr };
    QLineEdit* active_line_name_edit { nullptr };
    QCheckBox* active_line_closed_cb { nullptr };
    QPushButton* active_line_color_btn { nullptr };
    QColor active_line_color { Qt::red };
    QPushButton* active_line_undo_btn { nullptr };
    QPushButton* active_line_save_btn { nullptr };

    QGroupBox* active_templates_box { nullptr };
    QComboBox* active_template_combo { nullptr };
    QPushButton* active_template_color_btn { nullptr };
    QColor active_template_color { Qt::red };
    QPushButton* active_template_add_btn { nullptr };

    QPlainTextEdit* active_log_view = nullptr;
};

#endif // YODAU_FRONTEND_WIDGETS_SETTINGS_PANEL_HPP
