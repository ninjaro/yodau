#ifndef YODAU_FRONTEND_WIDGETS_ALGORITHM_PANEL_HPP
#define YODAU_FRONTEND_WIDGETS_ALGORITHM_PANEL_HPP

#include "shell/frontend_settings.hpp"

#include <QGroupBox>

class QCheckBox;
class QComboBox;
class QLabel;

class algorithm_panel final : public QGroupBox {
    Q_OBJECT

public:
    explicit algorithm_panel(
        QString object_prefix = QStringLiteral("settings_active"),
        QWidget* parent = nullptr
    );

    void set_stream_settings(const stream_settings& settings_value);
    stream_settings current_stream_settings() const;
    void set_stream_active(bool active);

signals:
    void settings_changed(stream_settings settings_value);

private slots:
    void on_algorithm_changed(int index);
    void on_preset_changed(int index);
    void on_overlay_toggled(bool checked);

private:
    void build_ui();
    void refresh_preset_options();
    void refresh_summary();
    stream_settings normalized_settings(stream_settings settings_value) const;
    QString object_name(const QString& suffix) const;

    QComboBox* algorithm_combo { nullptr };
    QComboBox* preset_combo { nullptr };
    QCheckBox* overlay_checkbox { nullptr };
    QLabel* summary_label { nullptr };
    QString object_prefix_;
    stream_settings current_settings;
};

#endif // YODAU_FRONTEND_WIDGETS_ALGORITHM_PANEL_HPP
