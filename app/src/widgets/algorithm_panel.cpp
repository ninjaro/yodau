#include "widgets/algorithm_panel.hpp"
#include "shell/str_label.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <utility>

namespace algorithm_panel_support {

void populate_algorithm_combo(QComboBox* combo) {
    if (combo == nullptr) {
        return;
    }

    combo->clear();
    for (const QString& algorithm_id : app_algorithm_ids()) {
        combo->addItem(
            app_algorithm_display_name(algorithm_id), algorithm_id
        );
    }
}

void populate_preset_combo(QComboBox* combo, const QString& algorithm_id) {
    if (combo == nullptr) {
        return;
    }

    combo->clear();
    for (const QString& preset_id : algorithm_preset_ids(algorithm_id)) {
        combo->addItem(
            algorithm_preset_display_name(algorithm_id, preset_id), preset_id
        );
    }
}

void populate_display_mode_combo(QComboBox* combo) {
    if (combo == nullptr) {
        return;
    }

    combo->clear();
    for (const QString& mode_id : movement_display_mode_ids()) {
        combo->addItem(movement_display_mode_display_name(mode_id), mode_id);
    }
}

void set_form_row_visible(QFormLayout* form, QWidget* field, const bool visible) {
    if (field == nullptr) {
        return;
    }

    if (form != nullptr) {
        if (QWidget* label = form->labelForField(field); label != nullptr) {
            label->setVisible(visible);
        }
    }

    field->setVisible(visible);
}

} // namespace algorithm_panel_support

algorithm_panel::algorithm_panel(QString object_prefix, QWidget* parent)
    : QGroupBox(str_label("algorithm"), parent)
    , object_prefix_(std::move(object_prefix)) {
    build_ui();
    set_stream_settings(stream_settings {});
}

void algorithm_panel::set_stream_settings(const stream_settings& settings_value) {
    current_settings = normalized_settings(settings_value);

    if (algorithm_combo != nullptr) {
        const int algorithm_index
            = algorithm_combo->findData(current_settings.algorithm_id);
        QSignalBlocker blocker(algorithm_combo);
        algorithm_combo->setCurrentIndex(algorithm_index >= 0 ? algorithm_index : 0);
    }

    refresh_preset_options();

    if (display_mode_combo != nullptr) {
        QSignalBlocker blocker(display_mode_combo);
        const int mode_index = display_mode_combo->findData(
            current_settings.movement_display_mode
        );
        display_mode_combo->setCurrentIndex(mode_index >= 0 ? mode_index : 0);
    }

    refresh_advanced_controls();
    refresh_summary();
}

stream_settings algorithm_panel::current_stream_settings() const {
    return current_settings;
}

void algorithm_panel::set_stream_active(const bool active) {
    setVisible(active);
    setEnabled(active);
}

void algorithm_panel::on_algorithm_changed(const int index) {
    Q_UNUSED(index);

    if (algorithm_combo != nullptr) {
        current_settings.algorithm_id = normalized_app_algorithm_id(
            algorithm_combo->currentData().toString()
        );
    }
    current_settings.algorithm_preset
        = default_algorithm_preset_id(current_settings.algorithm_id);
    refresh_preset_options();
    refresh_summary();
    emit settings_changed(current_settings);
}

void algorithm_panel::on_preset_changed(const int index) {
    Q_UNUSED(index);

    if (preset_combo != nullptr) {
        current_settings.algorithm_preset = normalized_algorithm_preset_id(
            current_settings.algorithm_id, preset_combo->currentData().toString()
        );
    }
    refresh_summary();
    emit settings_changed(current_settings);
}

void algorithm_panel::on_advanced_toggled(const bool checked) {
    Q_UNUSED(checked);

    refresh_advanced_controls();
}

void algorithm_panel::on_display_mode_changed(const int index) {
    Q_UNUSED(index);

    if (display_mode_combo != nullptr) {
        current_settings.movement_display_mode = normalized_movement_display_mode_id(
            display_mode_combo->currentData().toString()
        );
        current_settings.algorithm_overlay_enabled = movement_display_enabled(
            current_settings.movement_display_mode
        );
    }
    refresh_summary();
    emit settings_changed(current_settings);
}

void algorithm_panel::build_ui() {
    const auto layout = new QVBoxLayout(this);

    form_ = new QFormLayout();

    algorithm_combo = new QComboBox(this);
    algorithm_combo->setObjectName(object_name(QStringLiteral("algorithm_combo")));
    algorithm_panel_support::populate_algorithm_combo(algorithm_combo);
    form_->addRow(str_label("algorithm"), algorithm_combo);

    advanced_checkbox = new QCheckBox(
        str_label("advanced algorithm settings"), this
    );
    advanced_checkbox->setObjectName(
        object_name(QStringLiteral("algorithm_advanced_checkbox"))
    );
    advanced_checkbox->setToolTip(
        QStringLiteral(
            "Show or hide algorithm-specific preset and overlay controls. "
            "Leave this off to keep the shared stream settings compact."
        )
    );
    form_->addRow(QString(), advanced_checkbox);

    preset_combo = new QComboBox(this);
    preset_combo->setObjectName(object_name(QStringLiteral("algorithm_preset_combo")));
    form_->addRow(str_label("preset"), preset_combo);

    display_mode_combo = new QComboBox(this);
    display_mode_combo->setObjectName(
        object_name(QStringLiteral("movement_display_combo"))
    );
    algorithm_panel_support::populate_display_mode_combo(display_mode_combo);
    form_->addRow(str_label("movement display"), display_mode_combo);

    layout->addLayout(form_);

    summary_label = new QLabel(this);
    summary_label->setObjectName(object_name(QStringLiteral("algorithm_summary_label")));
    summary_label->setWordWrap(true);
    layout->addWidget(summary_label);

    connect(
        algorithm_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &algorithm_panel::on_algorithm_changed
    );
    connect(
        advanced_checkbox, &QCheckBox::toggled, this,
        &algorithm_panel::on_advanced_toggled
    );
    connect(
        preset_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &algorithm_panel::on_preset_changed
    );
    connect(
        display_mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &algorithm_panel::on_display_mode_changed
    );

    setLayout(layout);
}

void algorithm_panel::refresh_advanced_controls() const {
    const bool advanced_enabled = advanced_checkbox != nullptr
        && advanced_checkbox->isChecked();

    algorithm_panel_support::set_form_row_visible(
        form_, preset_combo, advanced_enabled
    );

    algorithm_panel_support::set_form_row_visible(
        form_, display_mode_combo, advanced_enabled
    );
    if (display_mode_combo != nullptr) {
        display_mode_combo->setEnabled(advanced_enabled);
    }
}

void algorithm_panel::refresh_preset_options() {
    if (preset_combo == nullptr) {
        return;
    }

    QSignalBlocker blocker(preset_combo);
    algorithm_panel_support::populate_preset_combo(
        preset_combo, current_settings.algorithm_id
    );

    const int preset_index = preset_combo->findData(
        current_settings.algorithm_preset
    );
    preset_combo->setCurrentIndex(preset_index >= 0 ? preset_index : 0);
}

void algorithm_panel::refresh_summary() {
    if (summary_label == nullptr) {
        return;
    }

    summary_label->setText(
        algorithm_summary_text(
            current_settings.algorithm_id, current_settings.algorithm_preset,
            current_settings.movement_display_mode
        )
    );
}

stream_settings
algorithm_panel::normalized_settings(stream_settings settings_value) const {
    settings_value.algorithm_id = normalized_app_algorithm_id(
        settings_value.algorithm_id
    );
    settings_value.algorithm_preset = normalized_algorithm_preset_id(
        settings_value.algorithm_id, settings_value.algorithm_preset
    );
    settings_value.movement_display_mode = normalized_movement_display_mode_id(
        settings_value.movement_display_mode
    );
    settings_value.algorithm_overlay_enabled = movement_display_enabled(
        settings_value.movement_display_mode
    );
    return settings_value;
}

QString algorithm_panel::object_name(const QString& suffix) const {
    return QStringLiteral("%1_%2").arg(object_prefix_, suffix);
}
