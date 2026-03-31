#ifndef YODAU_FRONTEND_WIDGETS_ACTIVE_STREAM_PANEL_HPP
#define YODAU_FRONTEND_WIDGETS_ACTIVE_STREAM_PANEL_HPP

#include "shell/frontend_settings.hpp"

#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QRadioButton;
class algorithm_panel;

class active_stream_panel final : public QWidget {
    Q_OBJECT

public:
    explicit active_stream_panel(QWidget* parent = nullptr);

    void set_active_candidates(const QStringList& names) const;
    void set_active_current(const QString& name) const;
    void set_stream_settings(const stream_settings& settings_value);
    stream_settings current_stream_settings() const;

    bool has_active_stream() const;
    bool drawing_new_mode() const;

signals:
    void stream_settings_changed(stream_settings settings_value);
    void edit_mode_changed(bool drawing_new);

private slots:
    void on_active_combo_changed(const QString& text);
    void on_active_labels_toggled(bool checked);
    void on_algorithm_panel_settings_changed(stream_settings settings_value);
    void on_active_mode_clicked(int id);

private:
    void build_ui();
    void refresh_panel_state() const;

    QComboBox* active_combo { nullptr };
    QCheckBox* active_labels_cb { nullptr };
    algorithm_panel* active_algorithm_panel { nullptr };

    QGroupBox* active_mode_box { nullptr };
    QButtonGroup* active_mode_group { nullptr };
    QRadioButton* active_mode_draw_radio { nullptr };
    QRadioButton* active_mode_template_radio { nullptr };
};

#endif // YODAU_FRONTEND_WIDGETS_ACTIVE_STREAM_PANEL_HPP
