#ifndef YODAU_APP_WIDGETS_LINE_PROFILE_PANEL_HPP
#define YODAU_APP_WIDGETS_LINE_PROFILE_PANEL_HPP

#include "shell/app_settings.hpp"

#include <QColor>
#include <QGroupBox>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

class line_profile_panel final : public QGroupBox {
    Q_OBJECT

public:
    explicit line_profile_panel(QWidget* parent = nullptr);

    void set_line_profile(const line_profile& profile);
    [[nodiscard]] line_profile current_line_profile() const;
    void reset_form();
    void set_panel_active(bool active);
    void set_line_closed(bool closed);

signals:
    void profile_changed(line_profile profile_value);
    void save_requested(line_profile profile_value);
    void undo_requested();

private slots:
    void on_color_clicked();
    void on_random_color_clicked();
    void on_undo_clicked();
    void on_save_clicked();
    void on_name_finished();
    void on_color_mode_changed(int index);
    void on_advanced_settings_toggled(bool checked);
    void on_parameter_mode_changed(int index);
    void on_width_changed(const QString& text);
    void on_length_changed(const QString& text);
    void on_response_changed(const QString& text);
    void on_closed_toggled(bool checked);

private:
    void build_ui();
    [[nodiscard]] QString current_color_mode_id() const;
    [[nodiscard]] QString current_parameter_mode_id() const;
    [[nodiscard]] bool advanced_settings_enabled() const;
    void refresh_color_controls();
    void refresh_advanced_settings_controls();
    void refresh_parameter_mode_labels();
    void refresh_summary();

    QLineEdit* name_edit { nullptr };
    QCheckBox* closed_checkbox { nullptr };
    QCheckBox* advanced_settings_checkbox { nullptr };
    QPushButton* color_button { nullptr };
    QPushButton* random_color_button { nullptr };
    QComboBox* color_mode_combo { nullptr };
    QComboBox* parameter_mode_combo { nullptr };
    QLabel* parameter_mode_hint_label { nullptr };
    QLabel* width_label { nullptr };
    QComboBox* width_combo { nullptr };
    QLabel* length_label { nullptr };
    QComboBox* length_combo { nullptr };
    QLabel* response_label { nullptr };
    QComboBox* response_combo { nullptr };
    QPushButton* undo_button { nullptr };
    QPushButton* save_button { nullptr };
    QLabel* summary_label { nullptr };
    QColor current_color { Qt::red };
};

#endif // YODAU_APP_WIDGETS_LINE_PROFILE_PANEL_HPP
