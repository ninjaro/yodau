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

signals:
    void add_file_stream(const QString& path, const QString& name, bool loop);
    void add_local_stream(const QString& source, const QString& name);
    void add_url_stream(const QString& url, const QString& name);

    void detect_local_sources_requested();
    void show_stream_changed(const QString& name, bool show);

private:
    enum class input_mode { file, local, url };

    void build_ui();
    QWidget* build_add_tab();
    QWidget* build_streams_tab();

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
};

#endif // YODAU_FRONTEND_WIDGETS_SETTINGS_PANEL_HPP