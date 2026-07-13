#ifndef YODAU_APP_WIDGETS_ACTIVE_STREAM_PANEL_HPP
#define YODAU_APP_WIDGETS_ACTIVE_STREAM_PANEL_HPP

#include "shell/app_settings.hpp"

#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QRadioButton;
class QSpinBox;
class algorithm_panel;

class active_stream_panel final : public QWidget {
    Q_OBJECT

public:
    enum class panel_mode {
        stream_settings,
        line_editor,
    };

    explicit active_stream_panel(
        panel_mode mode, QString object_prefix, QWidget* parent = nullptr
    );

    void set_active_candidates(const QStringList& names) const;
    void set_active_current(const QString& name) const;
    void set_stream_settings(const stream_settings& settings_value);
    [[nodiscard]] stream_settings current_stream_settings() const;
    QWidget* take_edit_mode_widget();

    [[nodiscard]] bool has_active_stream() const;
    [[nodiscard]] bool drawing_new_mode() const;

signals:
    void stream_selected(const QString& name);
    void stream_settings_changed(stream_settings settings_value);
    void edit_mode_changed(bool drawing_new);

private slots:
    void on_active_combo_changed(const QString& text);
    void on_active_labels_toggled(bool checked);
    void on_standard_labels_toggled(bool checked);
    void on_operator_profile_changed(int index);
    void on_algorithm_panel_settings_changed(stream_settings settings_value);
    void on_active_mode_clicked(int id);
    void on_processing_advanced_toggled(bool checked);
    void on_manual_processing_toggled(bool checked);
    void on_manual_display_fps_changed(int value);
    void on_manual_core_fps_changed(int value);
    void on_manual_processing_pixels_changed(int value);

private:
    void build_ui();
    void refresh_panel_state() const;
    void
    sync_operator_profile_from_settings(const stream_settings& settings_value);
    void refresh_operator_profile_summary();
    void refresh_processing_policy_state() const;
    void refresh_processing_policy_summary();

    [[nodiscard]] QString object_name(const QString& suffix) const;

    QComboBox* active_combo { nullptr };
    QCheckBox* active_labels_cb { nullptr };
    QCheckBox* standard_labels_cb { nullptr };
    QGroupBox* operator_profile_box { nullptr };
    QComboBox* operator_profile_combo { nullptr };
    QLabel* operator_profile_summary_label { nullptr };
    algorithm_panel* active_algorithm_panel { nullptr };
    QGroupBox* processing_policy_box { nullptr };
    QFormLayout* processing_policy_form_ { nullptr };
    QCheckBox* processing_advanced_checkbox { nullptr };
    QCheckBox* manual_processing_checkbox { nullptr };
    QSpinBox* manual_display_fps_spin { nullptr };
    QSpinBox* manual_core_fps_spin { nullptr };
    QSpinBox* manual_processing_pixels_spin { nullptr };
    QLabel* processing_summary_label { nullptr };

    QGroupBox* active_mode_box { nullptr };
    QButtonGroup* active_mode_group { nullptr };
    QRadioButton* active_mode_draw_radio { nullptr };
    QRadioButton* active_mode_template_radio { nullptr };
    panel_mode mode_ { panel_mode::stream_settings };
    QString object_prefix_;
    QString last_algorithm_id_ { default_app_algorithm_id() };
};

#endif // YODAU_APP_WIDGETS_ACTIVE_STREAM_PANEL_HPP
