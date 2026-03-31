#include "widgets/active_stream_panel.hpp"

#include "shell/str_label.hpp"
#include "widgets/algorithm_panel.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace active_stream_panel_support {

QString none_text() { return str_label("none"); }

} // namespace active_stream_panel_support

active_stream_panel::active_stream_panel(QWidget* parent)
    : QWidget(parent) {
    build_ui();
    set_stream_settings(stream_settings {});
    refresh_panel_state();
}

void active_stream_panel::set_active_candidates(const QStringList& names) const {
    if (active_combo == nullptr) {
        return;
    }

    const QString none_text = active_stream_panel_support::none_text();
    const QString current = (active_combo->currentText() == none_text)
        ? QString()
        : active_combo->currentText();

    QStringList final_names = names;
    if (!current.isEmpty() && !final_names.contains(current)) {
        final_names.prepend(current);
    }

    QSignalBlocker blocker(active_combo);
    active_combo->clear();
    active_combo->addItem(none_text, QVariant());

    for (const QString& name : final_names) {
        if (name.isEmpty() || name == none_text) {
            continue;
        }
        active_combo->addItem(name);
    }

    if (!current.isEmpty() && final_names.contains(current)) {
        active_combo->setCurrentText(current);
    } else {
        active_combo->setCurrentText(none_text);
    }

    refresh_panel_state();
}

void active_stream_panel::set_active_current(const QString& name) const {
    if (active_combo == nullptr) {
        return;
    }

    const QString none_text = active_stream_panel_support::none_text();
    QSignalBlocker blocker(active_combo);
    if (name.isEmpty() || active_combo->findText(name) < 0) {
        active_combo->setCurrentText(none_text);
    } else {
        active_combo->setCurrentText(name);
    }

    refresh_panel_state();
}

void active_stream_panel::set_stream_settings(
    const stream_settings& settings_value
) {
    const QString stream_name = settings_value.stream_name.trimmed();
    const QString none_text = active_stream_panel_support::none_text();

    if (active_combo != nullptr) {
        QSignalBlocker blocker(active_combo);
        if (stream_name.isEmpty() || active_combo->findText(stream_name) < 0) {
            active_combo->setCurrentText(none_text);
        } else {
            active_combo->setCurrentText(stream_name);
        }
    }

    if (active_labels_cb != nullptr) {
        QSignalBlocker blocker(active_labels_cb);
        active_labels_cb->setChecked(settings_value.labels_enabled);
    }

    if (active_algorithm_panel != nullptr) {
        active_algorithm_panel->set_stream_settings(settings_value);
    }

    refresh_panel_state();
}

stream_settings active_stream_panel::current_stream_settings() const {
    stream_settings settings_value;

    if (active_combo != nullptr) {
        const QString selected_name = active_combo->currentText().trimmed();
        if (selected_name != active_stream_panel_support::none_text()) {
            settings_value.stream_name = selected_name;
        }
    }

    if (active_labels_cb != nullptr) {
        settings_value.labels_enabled = active_labels_cb->isChecked();
    }

    if (active_algorithm_panel != nullptr) {
        const stream_settings algorithm_settings
            = active_algorithm_panel->current_stream_settings();
        settings_value.algorithm_id = algorithm_settings.algorithm_id;
        settings_value.algorithm_preset = algorithm_settings.algorithm_preset;
        settings_value.algorithm_overlay_enabled
            = algorithm_settings.algorithm_overlay_enabled;
    } else {
        settings_value.algorithm_id = default_frontend_algorithm_id();
        settings_value.algorithm_preset = default_algorithm_preset_id(
            settings_value.algorithm_id
        );
    }

    return settings_value;
}

bool active_stream_panel::has_active_stream() const {
    return active_combo != nullptr
        && active_combo->currentText().trimmed()
            != active_stream_panel_support::none_text();
}

bool active_stream_panel::drawing_new_mode() const {
    return active_mode_draw_radio == nullptr || active_mode_draw_radio->isChecked();
}

void active_stream_panel::on_active_combo_changed(const QString& text) {
    Q_UNUSED(text);

    refresh_panel_state();
    emit stream_settings_changed(current_stream_settings());
}

void active_stream_panel::on_active_labels_toggled(const bool checked) {
    Q_UNUSED(checked);

    emit stream_settings_changed(current_stream_settings());
}

void active_stream_panel::on_algorithm_panel_settings_changed(
    stream_settings settings_value
) {
    Q_UNUSED(settings_value);

    emit stream_settings_changed(current_stream_settings());
}

void active_stream_panel::on_active_mode_clicked(const int id) {
    refresh_panel_state();
    emit edit_mode_changed(id == 0);
}

void active_stream_panel::build_ui() {
    setObjectName(QStringLiteral("settings_active_stream_panel"));

    const auto layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    const auto active_stream_box
        = new QGroupBox(str_label("active stream"), this);
    const auto active_stream_layout = new QVBoxLayout(active_stream_box);

    active_combo = new QComboBox(active_stream_box);
    active_combo->setEditable(false);
    active_combo->setObjectName(QStringLiteral("settings_active_stream_combo"));
    active_combo->addItem(active_stream_panel_support::none_text(), QVariant());
    active_stream_layout->addWidget(active_combo);

    active_labels_cb = new QCheckBox(str_label("labels"), active_stream_box);
    active_labels_cb->setObjectName(
        QStringLiteral("settings_active_labels_checkbox")
    );
    active_labels_cb->setChecked(true);
    active_stream_layout->addWidget(active_labels_cb);

    layout->addWidget(active_stream_box);

    active_algorithm_panel = new algorithm_panel(this);
    active_algorithm_panel->setObjectName(
        QStringLiteral("settings_active_algorithm_panel")
    );
    layout->addWidget(active_algorithm_panel);

    active_mode_box = new QGroupBox(str_label("edit mode"), this);
    active_mode_box->setObjectName(QStringLiteral("settings_active_mode_box"));
    const auto mode_layout = new QHBoxLayout(active_mode_box);

    active_mode_group = new QButtonGroup(active_mode_box);
    active_mode_draw_radio
        = new QRadioButton(str_label("draw new"), active_mode_box);
    active_mode_draw_radio->setObjectName(
        QStringLiteral("settings_active_mode_draw_radio")
    );
    active_mode_template_radio
        = new QRadioButton(str_label("use template"), active_mode_box);
    active_mode_template_radio->setObjectName(
        QStringLiteral("settings_active_mode_template_radio")
    );

    active_mode_group->addButton(active_mode_draw_radio, 0);
    active_mode_group->addButton(active_mode_template_radio, 1);
    active_mode_draw_radio->setChecked(true);

    mode_layout->addWidget(active_mode_draw_radio);
    mode_layout->addWidget(active_mode_template_radio);
    layout->addWidget(active_mode_box);

    connect(
        active_combo, &QComboBox::currentTextChanged, this,
        &active_stream_panel::on_active_combo_changed
    );
    connect(
        active_labels_cb, &QCheckBox::toggled, this,
        &active_stream_panel::on_active_labels_toggled
    );
    connect(
        active_algorithm_panel, &algorithm_panel::settings_changed, this,
        &active_stream_panel::on_algorithm_panel_settings_changed
    );
    connect(
        active_mode_group, &QButtonGroup::idClicked, this,
        &active_stream_panel::on_active_mode_clicked
    );
}

void active_stream_panel::refresh_panel_state() const {
    const bool active = has_active_stream();

    if (active_mode_box != nullptr) {
        active_mode_box->setVisible(active);
        active_mode_box->setEnabled(active);
    }

    if (active_algorithm_panel != nullptr) {
        active_algorithm_panel->set_stream_active(active);
    }
}
