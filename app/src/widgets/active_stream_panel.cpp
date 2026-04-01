#include "widgets/active_stream_panel.hpp"

#include "shell/str_label.hpp"
#include "widgets/algorithm_panel.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

namespace active_stream_panel_support {

QString none_text() { return str_label("none"); }

void populate_operator_profile_combo(QComboBox* combo) {
    if (combo == nullptr) {
        return;
    }

    combo->clear();
    for (const QString& profile_id : operator_profile_ids(true)) {
        combo->addItem(
            operator_profile_display_name(profile_id), profile_id
        );
    }
}

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

    sync_operator_profile_from_settings(settings_value);

    if (manual_processing_checkbox != nullptr) {
        QSignalBlocker blocker(manual_processing_checkbox);
        manual_processing_checkbox->setChecked(
            settings_value.manual_processing_policy_enabled
        );
    }
    if (manual_display_fps_spin != nullptr) {
        QSignalBlocker blocker(manual_display_fps_spin);
        manual_display_fps_spin->setValue(settings_value.manual_display_fps);
    }
    if (manual_backend_fps_spin != nullptr) {
        QSignalBlocker blocker(manual_backend_fps_spin);
        manual_backend_fps_spin->setValue(settings_value.manual_backend_fps);
    }
    if (manual_processing_pixels_spin != nullptr) {
        QSignalBlocker blocker(manual_processing_pixels_spin);
        manual_processing_pixels_spin->setValue(
            settings_value.manual_processing_pixels
        );
    }

    refresh_panel_state();
    refresh_operator_profile_summary();
    refresh_processing_policy_summary();
    last_algorithm_id_ = normalized_frontend_algorithm_id(
        current_stream_settings().algorithm_id
    );
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

    if (manual_processing_checkbox != nullptr) {
        settings_value.manual_processing_policy_enabled
            = manual_processing_checkbox->isChecked();
    }
    settings_value.manual_display_fps = manual_display_fps_spin != nullptr
        ? manual_display_fps_spin->value()
        : default_manual_display_fps();
    settings_value.manual_backend_fps = manual_backend_fps_spin != nullptr
        ? manual_backend_fps_spin->value()
        : default_manual_backend_fps();
    settings_value.manual_processing_pixels
        = manual_processing_pixels_spin != nullptr
        ? manual_processing_pixels_spin->value()
        : default_manual_processing_pixels();

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
    refresh_operator_profile_summary();
    const stream_settings settings_value = current_stream_settings();
    last_algorithm_id_ = normalized_frontend_algorithm_id(
        settings_value.algorithm_id
    );
    emit stream_settings_changed(settings_value);
}

void active_stream_panel::on_active_labels_toggled(const bool checked) {
    Q_UNUSED(checked);

    emit stream_settings_changed(current_stream_settings());
}

void active_stream_panel::on_operator_profile_changed(const int index) {
    Q_UNUSED(index);

    if (operator_profile_combo == nullptr || active_algorithm_panel == nullptr
        || !has_active_stream()) {
        refresh_operator_profile_summary();
        return;
    }

    const QString profile_id = operator_profile_combo->currentData().toString();
    if (normalized_operator_profile_id(profile_id)
        == QStringLiteral("custom")) {
        refresh_operator_profile_summary();
        return;
    }

    const stream_settings updated_settings = apply_operator_profile(
        current_stream_settings(), profile_id
    );
    active_algorithm_panel->set_stream_settings(updated_settings);
    sync_operator_profile_from_settings(updated_settings);
    refresh_operator_profile_summary();
    last_algorithm_id_ = normalized_frontend_algorithm_id(
        updated_settings.algorithm_id
    );
    emit stream_settings_changed(current_stream_settings());
}

void active_stream_panel::on_algorithm_panel_settings_changed(
    stream_settings settings_value
) {
    if (operator_profile_combo != nullptr
        && normalized_operator_profile_id(
               operator_profile_combo->currentData().toString()
           )
            != QStringLiteral("custom")
        && normalized_frontend_algorithm_id(settings_value.algorithm_id)
            != last_algorithm_id_) {
        settings_value = apply_operator_profile(
            settings_value, operator_profile_combo->currentData().toString()
        );
        active_algorithm_panel->set_stream_settings(settings_value);
    }

    sync_operator_profile_from_settings(settings_value);
    refresh_operator_profile_summary();
    last_algorithm_id_ = normalized_frontend_algorithm_id(
        settings_value.algorithm_id
    );
    emit stream_settings_changed(current_stream_settings());
}

void active_stream_panel::on_active_mode_clicked(const int id) {
    refresh_panel_state();
    emit edit_mode_changed(id == 0);
}

void active_stream_panel::on_manual_processing_toggled(const bool checked) {
    Q_UNUSED(checked);

    refresh_processing_policy_state();
    refresh_processing_policy_summary();
    emit stream_settings_changed(current_stream_settings());
}

void active_stream_panel::on_manual_display_fps_changed(const int value) {
    Q_UNUSED(value);

    refresh_processing_policy_summary();
    emit stream_settings_changed(current_stream_settings());
}

void active_stream_panel::on_manual_backend_fps_changed(const int value) {
    Q_UNUSED(value);

    refresh_processing_policy_summary();
    emit stream_settings_changed(current_stream_settings());
}

void active_stream_panel::on_manual_processing_pixels_changed(const int value) {
    Q_UNUSED(value);

    refresh_processing_policy_summary();
    emit stream_settings_changed(current_stream_settings());
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

    operator_profile_box = new QGroupBox(str_label("operator profile"), this);
    operator_profile_box->setObjectName(
        QStringLiteral("settings_active_operator_profile_box")
    );
    const auto operator_profile_form = new QFormLayout(operator_profile_box);

    operator_profile_combo = new QComboBox(operator_profile_box);
    operator_profile_combo->setObjectName(
        QStringLiteral("settings_active_operator_profile_combo")
    );
    active_stream_panel_support::populate_operator_profile_combo(
        operator_profile_combo
    );
    operator_profile_form->addRow(
        str_label("shared preset"), operator_profile_combo
    );

    operator_profile_summary_label = new QLabel(operator_profile_box);
    operator_profile_summary_label->setObjectName(
        QStringLiteral("settings_active_operator_profile_summary_label")
    );
    operator_profile_summary_label->setWordWrap(true);
    operator_profile_form->addRow(QString(), operator_profile_summary_label);

    layout->addWidget(operator_profile_box);
    layout->addWidget(active_algorithm_panel);

    processing_policy_box
        = new QGroupBox(str_label("processing budget"), this);
    processing_policy_box->setObjectName(
        QStringLiteral("settings_active_processing_policy_box")
    );
    const auto processing_form = new QFormLayout(processing_policy_box);

    manual_processing_checkbox = new QCheckBox(
        str_label("manual stream tuning"), processing_policy_box
    );
    manual_processing_checkbox->setObjectName(
        QStringLiteral("settings_active_manual_processing_checkbox")
    );
    processing_form->addRow(QString(), manual_processing_checkbox);

    manual_display_fps_spin = new QSpinBox(processing_policy_box);
    manual_display_fps_spin->setObjectName(
        QStringLiteral("settings_active_display_fps_spin")
    );
    manual_display_fps_spin->setRange(1, 120);
    manual_display_fps_spin->setValue(default_manual_display_fps());
    manual_display_fps_spin->setSuffix(QStringLiteral(" fps"));
    processing_form->addRow(
        str_label("display fps cap"), manual_display_fps_spin
    );

    manual_backend_fps_spin = new QSpinBox(processing_policy_box);
    manual_backend_fps_spin->setObjectName(
        QStringLiteral("settings_active_backend_fps_spin")
    );
    manual_backend_fps_spin->setRange(1, 120);
    manual_backend_fps_spin->setValue(default_manual_backend_fps());
    manual_backend_fps_spin->setSuffix(QStringLiteral(" fps"));
    processing_form->addRow(
        str_label("backend fps target"), manual_backend_fps_spin
    );

    manual_processing_pixels_spin = new QSpinBox(processing_policy_box);
    manual_processing_pixels_spin->setObjectName(
        QStringLiteral("settings_active_processing_pixels_spin")
    );
    manual_processing_pixels_spin->setRange(16 * 16, 7680 * 4320);
    manual_processing_pixels_spin->setSingleStep(25 * 1000);
    manual_processing_pixels_spin->setValue(default_manual_processing_pixels());
    manual_processing_pixels_spin->setSuffix(QStringLiteral(" px"));
    processing_form->addRow(
        str_label("processing pixels"), manual_processing_pixels_spin
    );

    processing_summary_label = new QLabel(processing_policy_box);
    processing_summary_label->setObjectName(
        QStringLiteral("settings_active_processing_summary_label")
    );
    processing_summary_label->setWordWrap(true);
    processing_form->addRow(QString(), processing_summary_label);

    layout->addWidget(processing_policy_box);

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
        operator_profile_combo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &active_stream_panel::on_operator_profile_changed
    );
    connect(
        active_algorithm_panel, &algorithm_panel::settings_changed, this,
        &active_stream_panel::on_algorithm_panel_settings_changed
    );
    connect(
        manual_processing_checkbox, &QCheckBox::toggled, this,
        &active_stream_panel::on_manual_processing_toggled
    );
    connect(
        manual_display_fps_spin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, &active_stream_panel::on_manual_display_fps_changed
    );
    connect(
        manual_backend_fps_spin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, &active_stream_panel::on_manual_backend_fps_changed
    );
    connect(
        manual_processing_pixels_spin,
        QOverload<int>::of(&QSpinBox::valueChanged), this,
        &active_stream_panel::on_manual_processing_pixels_changed
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

    if (operator_profile_box != nullptr) {
        operator_profile_box->setVisible(active);
        operator_profile_box->setEnabled(active);
    }

    if (processing_policy_box != nullptr) {
        processing_policy_box->setVisible(active);
        processing_policy_box->setEnabled(active);
    }

    refresh_processing_policy_state();
}

void active_stream_panel::sync_operator_profile_from_settings(
    const stream_settings& settings_value
) {
    if (operator_profile_combo == nullptr) {
        return;
    }

    const int profile_index = operator_profile_combo->findData(
        inferred_operator_profile_id(settings_value)
    );
    QSignalBlocker blocker(operator_profile_combo);
    operator_profile_combo->setCurrentIndex(profile_index >= 0 ? profile_index : 0);
}

void active_stream_panel::refresh_operator_profile_summary() {
    if (operator_profile_summary_label == nullptr) {
        return;
    }

    if (!has_active_stream()) {
        operator_profile_summary_label->setText(
            str_label("choose a stream to apply a shared preset")
        );
        return;
    }

    operator_profile_summary_label->setText(
        operator_profile_summary_text(current_stream_settings())
    );
}

void active_stream_panel::refresh_processing_policy_state() const {
    const bool active = has_active_stream();
    const bool manual_enabled = manual_processing_checkbox != nullptr
        && manual_processing_checkbox->isChecked();
    const bool enable_controls = active && manual_enabled;

    if (manual_display_fps_spin != nullptr) {
        manual_display_fps_spin->setEnabled(enable_controls);
    }
    if (manual_backend_fps_spin != nullptr) {
        manual_backend_fps_spin->setEnabled(enable_controls);
    }
    if (manual_processing_pixels_spin != nullptr) {
        manual_processing_pixels_spin->setEnabled(enable_controls);
    }
}

void active_stream_panel::refresh_processing_policy_summary() {
    if (processing_summary_label == nullptr) {
        return;
    }

    processing_summary_label->setText(
        stream_processing_policy_summary_text(current_stream_settings())
    );
}
