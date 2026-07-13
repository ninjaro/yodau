#ifndef YODAU_APP_WIDGETS_TEMPLATE_APPLY_PANEL_HPP
#define YODAU_APP_WIDGETS_TEMPLATE_APPLY_PANEL_HPP

#include "shell/app_settings.hpp"

#include <QColor>
#include <QGroupBox>

class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;

class template_apply_panel final : public QGroupBox {
    Q_OBJECT

public:
    explicit template_apply_panel(QWidget* parent = nullptr);

    void add_template_candidate(const QString& name);
    void set_template_candidates(const QStringList& names);
    void set_template_settings(const template_apply_settings& settings_value);
    [[nodiscard]] template_apply_settings current_template_settings() const;
    void reset_form();
    [[nodiscard]] QString current_template_name() const;
    [[nodiscard]] QColor preview_color() const;
    [[nodiscard]] bool has_template_candidates() const;
    void set_panel_active(bool active);

signals:
    void settings_changed(template_apply_settings settings_value);
    void add_requested(template_apply_settings settings_value);

private slots:
    void on_template_changed(const QString& text);
    void on_color_clicked();
    void on_random_color_clicked();
    void on_color_mode_changed(int index);
    void on_advanced_settings_toggled(bool checked);
    void on_parameter_mode_changed(int index);
    void on_width_changed(const QString& text);
    void on_length_changed(const QString& text);
    void on_response_changed(const QString& text);
    void on_add_clicked();

private:
    void build_ui();
    [[nodiscard]] QString current_color_mode_id() const;
    [[nodiscard]] QString current_parameter_mode_id() const;
    [[nodiscard]] bool advanced_settings_enabled() const;
    void refresh_color_controls();
    void refresh_advanced_settings_controls();
    void refresh_parameter_mode_labels();
    void refresh_summary();

    QComboBox* template_combo { nullptr };
    QPushButton* color_button { nullptr };
    QPushButton* random_color_button { nullptr };
    QComboBox* color_mode_combo { nullptr };
    QCheckBox* advanced_settings_checkbox { nullptr };
    QComboBox* parameter_mode_combo { nullptr };
    QLabel* parameter_mode_hint_label { nullptr };
    QLabel* width_label { nullptr };
    QComboBox* width_combo { nullptr };
    QLabel* length_label { nullptr };
    QComboBox* length_combo { nullptr };
    QLabel* response_label { nullptr };
    QComboBox* response_combo { nullptr };
    QPushButton* add_button { nullptr };
    QLabel* summary_label { nullptr };
    QColor current_color { Qt::red };
};

#endif // YODAU_APP_WIDGETS_TEMPLATE_APPLY_PANEL_HPP
