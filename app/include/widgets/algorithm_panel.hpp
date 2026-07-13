#ifndef YODAU_APP_WIDGETS_ALGORITHM_PANEL_HPP
#define YODAU_APP_WIDGETS_ALGORITHM_PANEL_HPP

#include "shell/app_settings.hpp"

#include <QGroupBox>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;

class algorithm_panel final : public QGroupBox {
    Q_OBJECT

public:
    explicit algorithm_panel(
        QString object_prefix = QStringLiteral("settings_active"),
        QWidget* parent = nullptr
    );

    void set_stream_settings(const stream_settings& settings_value);
    [[nodiscard]] stream_settings current_stream_settings() const;
    void set_stream_active(bool active);

signals:
    void settings_changed(stream_settings settings_value);

private slots:
    void on_algorithm_changed(int index);
    void on_preset_changed(int index);
    void on_advanced_toggled(bool checked);
    void on_display_mode_changed(int index);

private:
    void build_ui();
    void refresh_advanced_controls() const;
    void refresh_preset_options();
    void refresh_summary();
    [[nodiscard]] static stream_settings
    normalized_settings(stream_settings settings_value);
    [[nodiscard]] QString object_name(const QString& suffix) const;

    QComboBox* algorithm_combo { nullptr };
    QFormLayout* form_ { nullptr };
    QCheckBox* advanced_checkbox { nullptr };
    QComboBox* preset_combo { nullptr };
    QComboBox* display_mode_combo { nullptr };
    QLabel* summary_label { nullptr };
    QString object_prefix_;
    stream_settings current_settings;
};

#endif // YODAU_APP_WIDGETS_ALGORITHM_PANEL_HPP
