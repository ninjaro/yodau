#ifndef YODAU_FRONTEND_WIDGETS_SETTINGS_PANEL_HPP
#define YODAU_FRONTEND_WIDGETS_SETTINGS_PANEL_HPP
#include <QSet>
#include <QWidget>

class QTabWidget;
class QLineEdit;
class QCheckBox;
class QComboBox;
class QPushButton;
class QRadioButton;
class QButtonGroup;
class QPlainTextEdit;
class QTreeWidget;
class QTreeWidgetItem;

class settings_panel final : public QWidget {
    Q_OBJECT
public:
    explicit settings_panel(QWidget* parent = nullptr);

    void set_existing_names(QSet<QString> names);
    void add_existing_name(const QString& name);
    void remove_existing_name(const QString& name);

    void add_stream_entry(
        const QString& name, const QString& source, bool checked = false
    ) const;
    void set_stream_checked(const QString& name, bool checked) const;
    void remove_stream_entry(const QString& name) const;
    void clear_stream_entries();

    void append_event(const QString& text) const;
    void append_add_log(const QString& text) const;

    void set_local_sources(const QStringList& sources) const;
    void clear_add_inputs() const;

    void set_active_candidates(const QStringList& names) const;
    void set_active_current(const QString& name) const;

    void add_template_candidate(const QString& name) const;
signals:
    void add_file_stream(const QString& path, const QString& name, bool loop);
    void add_local_stream(const QString& source, const QString& name);
    void add_url_stream(const QString& url, const QString& name);

    void detect_local_sources_requested();
    void show_stream_changed(const QString& name, bool show);

    void active_stream_selected(const QString& name);

    void active_line_params_changed(
        const QString& name, const QColor& color, bool closed
    );
    void active_line_save_requested(const QString& name, bool closed);
    void active_template_add_requested(
        const QString& template_name, const QColor& color
    );
    void active_edit_mode_changed(bool drawing_new);

private:
    enum class input_mode { file, local, url };

    void build_ui();
    QWidget* build_add_tab();
    QWidget* build_streams_tab();
    QWidget* build_active_tab();

    void set_mode(input_mode mode);
    void update_add_enabled() const;

    void on_choose_file();
    void on_add_clicked();
    void on_refresh_local();
    void on_name_changed(QString text) const;

    QString resolved_name_for_current_input() const;
    bool name_is_unique(const QString& name) const;
    bool current_input_valid() const;

    void set_name_error(bool error) const;
    void update_active_tools() const;

    QTabWidget* tabs;

    QWidget* add_tab;
    QLineEdit* name_edit;

    QButtonGroup* mode_group;
    QRadioButton* file_radio;
    QRadioButton* local_radio;
    QRadioButton* url_radio;
    input_mode current_mode;

    QLineEdit* file_path_edit;
    QPushButton* choose_file_btn;
    QCheckBox* loop_checkbox;

    QComboBox* local_sources_combo;
    QPushButton* refresh_local_btn;

    QLineEdit* url_edit;

    QPushButton* add_btn;
    QPlainTextEdit* add_log_view;

    QWidget* streams_tab;
    QTreeWidget* streams_list;
    QPlainTextEdit* event_log_view;

    QSet<QString> existing_names;

    QWidget* active_tab { nullptr };
    QComboBox* active_combo { nullptr };

    QLineEdit* active_line_name_edit { nullptr };
    QCheckBox* active_line_closed_cb { nullptr };
    QPushButton* active_line_color_btn { nullptr };
    QColor active_line_color { Qt::red };
    QPushButton* active_line_save_btn { nullptr };

    QComboBox* active_template_combo { nullptr };
    QPushButton* active_template_color_btn { nullptr };
    QColor active_template_color { Qt::red };
    QPushButton* active_template_add_btn { nullptr };

    QGroupBox* active_line_box { nullptr };
    QGroupBox* active_templates_box { nullptr };

    QButtonGroup* active_mode_group { nullptr };
    QRadioButton* active_mode_draw_radio { nullptr };
    QRadioButton* active_mode_template_radio { nullptr };
};

#endif // YODAU_FRONTEND_WIDGETS_SETTINGS_PANEL_HPP