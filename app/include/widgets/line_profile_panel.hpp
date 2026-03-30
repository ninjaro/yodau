#ifndef YODAU_FRONTEND_WIDGETS_LINE_PROFILE_PANEL_HPP
#define YODAU_FRONTEND_WIDGETS_LINE_PROFILE_PANEL_HPP

#include "shell/frontend_settings.hpp"

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
    line_profile current_line_profile() const;
    void reset_form();
    void set_panel_active(bool active);
    void set_line_closed(bool closed);

signals:
    void profile_changed(line_profile profile_value);
    void save_requested(line_profile profile_value);
    void undo_requested();

private slots:
    void on_color_clicked();
    void on_undo_clicked();
    void on_save_clicked();
    void on_name_finished();
    void on_width_changed(const QString& text);
    void on_length_changed(const QString& text);
    void on_response_changed(const QString& text);
    void on_closed_toggled(bool checked);

private:
    void build_ui();
    void refresh_summary();

    QLineEdit* name_edit { nullptr };
    QCheckBox* closed_checkbox { nullptr };
    QPushButton* color_button { nullptr };
    QComboBox* width_combo { nullptr };
    QComboBox* length_combo { nullptr };
    QComboBox* response_combo { nullptr };
    QPushButton* undo_button { nullptr };
    QPushButton* save_button { nullptr };
    QLabel* summary_label { nullptr };
    QColor current_color { Qt::red };
};

#endif // YODAU_FRONTEND_WIDGETS_LINE_PROFILE_PANEL_HPP
