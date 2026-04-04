#ifndef YODAU_APP_WIDGETS_STREAM_SOURCE_PANEL_HPP
#define YODAU_APP_WIDGETS_STREAM_SOURCE_PANEL_HPP

#include "shell/app_log.hpp"

#include <QSet>
#include <QStringList>
#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QLabel;

class stream_source_panel final : public QWidget {
    Q_OBJECT

public:
    explicit stream_source_panel(QWidget* parent = nullptr);

    void set_existing_names(QSet<QString> names);
    void add_existing_name(const QString& name);
    void remove_existing_name(const QString& name);

    void set_local_sources(const QStringList& sources) const;
    void clear_inputs() const;

signals:
    void add_file_stream(const QString& path, const QString& name, bool loop);
    void add_local_stream(const QString& source, const QString& name);
    void add_url_stream(const QString& url, const QString& name);
    void detect_local_sources_requested();
    void log_requested(app_log_entry entry);

private:
    enum class input_mode { file, local, url };

    void build_ui();
    void set_mode(input_mode mode);
    void update_tools() const;
    void update_add_enabled() const;
    void refresh_summary() const;

    QString resolved_name_for_current_input() const;
    bool name_is_unique(const QString& name) const;
    bool current_input_valid() const;
    void set_name_error(bool error) const;

private slots:
    void on_choose_file();
    void on_add_clicked();
    void on_refresh_local();
    void on_name_changed(QString text) const;
    void on_mode_group_clicked(int id);
    void on_local_source_changed();
    void on_url_text_changed();

private:
    QSet<QString> existing_names;
    input_mode current_mode { input_mode::file };

    QLineEdit* name_edit { nullptr };
    QButtonGroup* mode_group { nullptr };
    QRadioButton* file_radio { nullptr };
    QRadioButton* local_radio { nullptr };
    QRadioButton* url_radio { nullptr };
    QLabel* summary_label { nullptr };

    QGroupBox* add_file_box { nullptr };
    QLineEdit* file_path_edit { nullptr };
    QPushButton* choose_file_btn { nullptr };
    QCheckBox* loop_checkbox { nullptr };

    QGroupBox* add_local_box { nullptr };
    QComboBox* local_sources_combo { nullptr };
    QPushButton* refresh_local_btn { nullptr };

    QGroupBox* add_url_box { nullptr };
    QLineEdit* url_edit { nullptr };

    QPushButton* add_btn { nullptr };
};

#endif // YODAU_APP_WIDGETS_STREAM_SOURCE_PANEL_HPP
