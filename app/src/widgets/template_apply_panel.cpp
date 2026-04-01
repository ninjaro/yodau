#include "widgets/template_apply_panel.hpp"
#include "shell/str_label.hpp"

#include <QColorDialog>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace template_apply_panel_support {

void populate_combo(QComboBox* combo, const QStringList& values) {
    if (combo == nullptr) {
        return;
    }

    combo->clear();
    for (const QString& value : values) {
        combo->addItem(value, value);
    }
}

void set_editable_combo_value(QComboBox* combo, const QString& value) {
    if (combo == nullptr) {
        return;
    }

    QSignalBlocker blocker(combo);
    combo->setCurrentText(value);
}

void set_button_color(QPushButton* button, const QColor& color) {
    if (button == nullptr) {
        return;
    }

    button->setStyleSheet(
        QStringLiteral("background-color: %1;").arg(color.name())
    );
}

} // namespace template_apply_panel_support

template_apply_panel::template_apply_panel(QWidget* parent)
    : QGroupBox(str_label("templates"), parent) {
    build_ui();
    set_template_settings(template_apply_settings {});
}

void template_apply_panel::add_template_candidate(const QString& name) {
    if (template_combo == nullptr) {
        return;
    }

    const QString trimmed_name = name.trimmed();
    if (trimmed_name.isEmpty()) {
        return;
    }

    for (int index = 0; index < template_combo->count(); index += 1) {
        if (template_combo->itemText(index) == trimmed_name) {
            refresh_summary();
            return;
        }
    }

    template_combo->addItem(trimmed_name);
    if (template_combo->count() == 2) {
        template_combo->setCurrentIndex(1);
    }

    refresh_summary();
}

void template_apply_panel::set_template_candidates(const QStringList& names) {
    if (template_combo == nullptr) {
        return;
    }

    const QString none_text = str_label("none");

    QSignalBlocker blocker(template_combo);
    template_combo->clear();
    template_combo->addItem(none_text, QVariant());

    QSet<QString> seen;
    for (const QString& name : names) {
        const QString trimmed_name = name.trimmed();
        if (trimmed_name.isEmpty() || trimmed_name == none_text
            || seen.contains(trimmed_name)) {
            continue;
        }

        seen.insert(trimmed_name);
        template_combo->addItem(trimmed_name);
    }

    template_combo->setCurrentIndex(0);
    refresh_summary();
}

void template_apply_panel::set_template_settings(
    const template_apply_settings& settings_value
) {
    if (template_combo == nullptr || color_button == nullptr
        || width_combo == nullptr || length_combo == nullptr
        || response_combo == nullptr) {
        return;
    }

    const QString template_name = settings_value.template_name.trimmed();
    const int template_index = template_name.isEmpty()
        ? template_combo->findText(str_label("none"))
        : template_combo->findText(template_name);

    {
        QSignalBlocker blocker(template_combo);
        template_combo->setCurrentIndex(template_index >= 0 ? template_index : 0);
    }

    current_color = settings_value.color.isValid()
        ? settings_value.color
        : QColor(Qt::red);
    template_apply_panel_support::set_button_color(color_button, current_color);
    template_apply_panel_support::set_editable_combo_value(
        width_combo, normalized_line_width_text(settings_value.width_text)
    );
    template_apply_panel_support::set_editable_combo_value(
        length_combo, normalized_line_length_text(settings_value.length_text)
    );
    template_apply_panel_support::set_editable_combo_value(
        response_combo, normalized_line_response_text(settings_value.response_text)
    );

    refresh_summary();
}

template_apply_settings template_apply_panel::current_template_settings() const {
    template_apply_settings settings_value;

    if (template_combo != nullptr) {
        const QString template_name = template_combo->currentText().trimmed();
        if (template_name != str_label("none")) {
            settings_value.template_name = template_name;
        }
    }

    settings_value.color = current_color;
    settings_value.width_text = width_combo != nullptr
        ? normalized_line_width_text(width_combo->currentText())
        : default_line_width_text();
    settings_value.length_text = length_combo != nullptr
        ? normalized_line_length_text(length_combo->currentText())
        : default_line_length_text();
    settings_value.response_text = response_combo != nullptr
        ? normalized_line_response_text(response_combo->currentText())
        : default_line_response_text();

    return settings_value;
}

void template_apply_panel::reset_form() {
    set_template_settings(template_apply_settings {});
}

QString template_apply_panel::current_template_name() const {
    if (template_combo == nullptr) {
        return {};
    }

    return template_combo->currentText().trimmed();
}

QColor template_apply_panel::preview_color() const { return current_color; }

bool template_apply_panel::has_template_candidates() const {
    return template_combo != nullptr && template_combo->count() > 1;
}

void template_apply_panel::set_panel_active(const bool active) {
    setVisible(active);
    setEnabled(active);
}

void template_apply_panel::on_template_changed(const QString& text) {
    Q_UNUSED(text);

    refresh_summary();
    emit settings_changed(current_template_settings());
}

void template_apply_panel::on_color_clicked() {
    const QColor color = QColorDialog::getColor(
        current_color, this, str_label("choose color")
    );
    if (!color.isValid()) {
        return;
    }

    current_color = color;
    template_apply_panel_support::set_button_color(color_button, current_color);
    refresh_summary();
    emit settings_changed(current_template_settings());
}

void template_apply_panel::on_parameter_mode_changed(const int index) {
    Q_UNUSED(index);

    refresh_parameter_mode_labels();
    refresh_summary();
}

void template_apply_panel::on_width_changed(const QString& text) {
    Q_UNUSED(text);

    refresh_summary();
    emit settings_changed(current_template_settings());
}

void template_apply_panel::on_length_changed(const QString& text) {
    Q_UNUSED(text);

    refresh_summary();
    emit settings_changed(current_template_settings());
}

void template_apply_panel::on_response_changed(const QString& text) {
    Q_UNUSED(text);

    refresh_summary();
    emit settings_changed(current_template_settings());
}

void template_apply_panel::on_add_clicked() {
    if (template_combo == nullptr) {
        return;
    }

    const QString current_text = template_combo->currentText();
    if (current_text.isEmpty() || current_text == str_label("none")) {
        return;
    }

    emit add_requested(current_template_settings());
}

void template_apply_panel::build_ui() {
    const auto layout = new QVBoxLayout(this);

    template_combo = new QComboBox(this);
    template_combo->setEditable(false);
    template_combo->setObjectName(QStringLiteral("settings_active_template_combo"));
    template_combo->addItem(str_label("none"), QVariant());
    layout->addWidget(template_combo);

    color_button = new QPushButton(str_label("color"), this);
    color_button->setObjectName(
        QStringLiteral("settings_active_template_color_button")
    );
    template_apply_panel_support::set_button_color(color_button, current_color);
    layout->addWidget(color_button);

    parameter_mode_combo = new QComboBox(this);
    parameter_mode_combo->setObjectName(
        QStringLiteral("settings_active_template_parameter_mode_combo")
    );
    for (const QString& mode_id : line_parameter_mode_ids()) {
        parameter_mode_combo->addItem(
            line_parameter_mode_display_name(mode_id), mode_id
        );
    }
    parameter_mode_combo->setCurrentIndex(0);
    layout->addWidget(parameter_mode_combo);

    parameter_mode_hint_label = new QLabel(this);
    parameter_mode_hint_label->setObjectName(
        QStringLiteral("settings_active_template_parameter_mode_hint_label")
    );
    parameter_mode_hint_label->setWordWrap(true);
    layout->addWidget(parameter_mode_hint_label);

    width_label = new QLabel(this);
    layout->addWidget(width_label);
    width_combo = new QComboBox(this);
    width_combo->setEditable(true);
    width_combo->setObjectName(
        QStringLiteral("settings_active_template_width_combo")
    );
    template_apply_panel_support::populate_combo(
        width_combo, suggested_line_width_texts()
    );
    width_combo->setCurrentText(default_line_width_text());
    layout->addWidget(width_combo);

    length_label = new QLabel(this);
    layout->addWidget(length_label);
    length_combo = new QComboBox(this);
    length_combo->setEditable(true);
    length_combo->setObjectName(
        QStringLiteral("settings_active_template_length_combo")
    );
    template_apply_panel_support::populate_combo(
        length_combo, suggested_line_length_texts()
    );
    length_combo->setCurrentText(default_line_length_text());
    layout->addWidget(length_combo);

    response_label = new QLabel(this);
    layout->addWidget(response_label);
    response_combo = new QComboBox(this);
    response_combo->setEditable(true);
    response_combo->setObjectName(
        QStringLiteral("settings_active_template_response_combo")
    );
    template_apply_panel_support::populate_combo(
        response_combo, suggested_line_response_texts()
    );
    response_combo->setCurrentText(default_line_response_text());
    layout->addWidget(response_combo);

    summary_label = new QLabel(this);
    summary_label->setObjectName(
        QStringLiteral("settings_active_template_summary_label")
    );
    summary_label->setWordWrap(true);
    layout->addWidget(summary_label);

    add_button = new QPushButton(str_label("add template"), this);
    add_button->setObjectName(QStringLiteral("settings_active_template_add_button"));
    layout->addWidget(add_button);

    connect(
        template_combo, &QComboBox::currentTextChanged, this,
        &template_apply_panel::on_template_changed
    );
    connect(
        color_button, &QPushButton::clicked, this,
        &template_apply_panel::on_color_clicked
    );
    connect(
        parameter_mode_combo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &template_apply_panel::on_parameter_mode_changed
    );
    connect(
        width_combo, &QComboBox::currentTextChanged, this,
        &template_apply_panel::on_width_changed
    );
    connect(
        length_combo, &QComboBox::currentTextChanged, this,
        &template_apply_panel::on_length_changed
    );
    connect(
        response_combo, &QComboBox::currentTextChanged, this,
        &template_apply_panel::on_response_changed
    );
    connect(
        add_button, &QPushButton::clicked, this,
        &template_apply_panel::on_add_clicked
    );

    setLayout(layout);
    refresh_parameter_mode_labels();
}

QString template_apply_panel::current_parameter_mode_id() const {
    return parameter_mode_combo != nullptr
        ? normalized_line_parameter_mode_id(
              parameter_mode_combo->currentData().toString()
          )
        : default_line_parameter_mode_id();
}

void template_apply_panel::refresh_parameter_mode_labels() {
    const QString mode_id = current_parameter_mode_id();

    if (width_label != nullptr) {
        width_label->setText(str_label("%1").arg(line_width_label_text(mode_id)));
    }
    if (length_label != nullptr) {
        length_label->setText(str_label("%1").arg(line_length_label_text(mode_id)));
    }
    if (response_label != nullptr) {
        response_label->setText(
            str_label("%1").arg(line_response_label_text(mode_id))
        );
    }
    if (parameter_mode_hint_label != nullptr) {
        parameter_mode_hint_label->setText(
            line_parameter_mode_hint_text(mode_id)
        );
    }
}

void template_apply_panel::refresh_summary() {
    if (summary_label == nullptr) {
        return;
    }

    const template_apply_settings settings_value = current_template_settings();
    QString text = line_profile_summary_text(
        settings_value.width_text, settings_value.length_text,
        settings_value.response_text, current_parameter_mode_id()
    );

    if (!settings_value.template_name.isEmpty()) {
        text = QStringLiteral("%1: %2").arg(settings_value.template_name, text);
    }

    summary_label->setText(text);
}
