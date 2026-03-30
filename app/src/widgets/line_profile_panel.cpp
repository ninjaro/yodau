#include "widgets/line_profile_panel.hpp"
#include "shell/str_label.hpp"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace line_profile_panel_support {

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

} // namespace line_profile_panel_support

line_profile_panel::line_profile_panel(QWidget* parent)
    : QGroupBox(str_label("line profile"), parent) {
    build_ui();
    set_line_profile(line_profile {});
}

void line_profile_panel::set_line_profile(const line_profile& profile) {
    if (name_edit == nullptr || closed_checkbox == nullptr
        || color_button == nullptr || width_combo == nullptr
        || length_combo == nullptr || response_combo == nullptr) {
        return;
    }

    {
        QSignalBlocker blocker(name_edit);
        name_edit->setText(profile.name.trimmed());
    }
    {
        QSignalBlocker blocker(closed_checkbox);
        closed_checkbox->setChecked(profile.closed);
    }

    current_color = profile.color.isValid() ? profile.color : QColor(Qt::red);
    line_profile_panel_support::set_button_color(color_button, current_color);
    line_profile_panel_support::set_editable_combo_value(
        width_combo, normalized_line_width_text(profile.width_text)
    );
    line_profile_panel_support::set_editable_combo_value(
        length_combo, normalized_line_length_text(profile.length_text)
    );
    line_profile_panel_support::set_editable_combo_value(
        response_combo, normalized_line_response_text(profile.response_text)
    );

    refresh_summary();
}

line_profile line_profile_panel::current_line_profile() const {
    line_profile profile;

    if (name_edit != nullptr) {
        profile.name = name_edit->text().trimmed();
    }

    profile.color = current_color;
    profile.closed
        = closed_checkbox != nullptr && closed_checkbox->isChecked();
    profile.width_text = width_combo != nullptr
        ? normalized_line_width_text(width_combo->currentText())
        : default_line_width_text();
    profile.length_text = length_combo != nullptr
        ? normalized_line_length_text(length_combo->currentText())
        : default_line_length_text();
    profile.response_text = response_combo != nullptr
        ? normalized_line_response_text(response_combo->currentText())
        : default_line_response_text();

    return profile;
}

void line_profile_panel::reset_form() { set_line_profile(line_profile {}); }

void line_profile_panel::set_panel_active(const bool active) {
    setVisible(active);
    setEnabled(active);
}

void line_profile_panel::set_line_closed(const bool closed) {
    if (closed_checkbox == nullptr) {
        return;
    }

    QSignalBlocker blocker(closed_checkbox);
    closed_checkbox->setChecked(closed);
    refresh_summary();
}

void line_profile_panel::on_color_clicked() {
    const QColor color = QColorDialog::getColor(
        current_color, this, str_label("choose color")
    );
    if (!color.isValid()) {
        return;
    }

    current_color = color;
    line_profile_panel_support::set_button_color(color_button, current_color);
    refresh_summary();
    emit profile_changed(current_line_profile());
}

void line_profile_panel::on_undo_clicked() { emit undo_requested(); }

void line_profile_panel::on_save_clicked() {
    emit save_requested(current_line_profile());
}

void line_profile_panel::on_name_finished() {
    refresh_summary();
    emit profile_changed(current_line_profile());
}

void line_profile_panel::on_width_changed(const QString& text) {
    Q_UNUSED(text);

    refresh_summary();
    emit profile_changed(current_line_profile());
}

void line_profile_panel::on_length_changed(const QString& text) {
    Q_UNUSED(text);

    refresh_summary();
    emit profile_changed(current_line_profile());
}

void line_profile_panel::on_response_changed(const QString& text) {
    Q_UNUSED(text);

    refresh_summary();
    emit profile_changed(current_line_profile());
}

void line_profile_panel::on_closed_toggled(const bool checked) {
    Q_UNUSED(checked);

    refresh_summary();
    emit profile_changed(current_line_profile());
}

void line_profile_panel::build_ui() {
    const auto layout = new QVBoxLayout(this);

    name_edit = new QLineEdit(this);
    name_edit->setObjectName(QStringLiteral("settings_active_line_name_edit"));
    name_edit->setPlaceholderText(str_label("template name (optional)"));
    layout->addWidget(name_edit);

    closed_checkbox = new QCheckBox(str_label("closed"), this);
    closed_checkbox->setObjectName(
        QStringLiteral("settings_active_line_closed_checkbox")
    );
    layout->addWidget(closed_checkbox);

    color_button = new QPushButton(str_label("color"), this);
    color_button->setObjectName(QStringLiteral("settings_active_line_color_button"));
    line_profile_panel_support::set_button_color(color_button, current_color);
    layout->addWidget(color_button);

    layout->addWidget(new QLabel(str_label("line width"), this));
    width_combo = new QComboBox(this);
    width_combo->setEditable(true);
    width_combo->setObjectName(QStringLiteral("settings_active_line_width_combo"));
    line_profile_panel_support::populate_combo(
        width_combo, suggested_line_width_texts()
    );
    width_combo->setCurrentText(default_line_width_text());
    layout->addWidget(width_combo);

    layout->addWidget(new QLabel(str_label("string length"), this));
    length_combo = new QComboBox(this);
    length_combo->setEditable(true);
    length_combo->setObjectName(
        QStringLiteral("settings_active_line_length_combo")
    );
    line_profile_panel_support::populate_combo(
        length_combo, suggested_line_length_texts()
    );
    length_combo->setCurrentText(default_line_length_text());
    layout->addWidget(length_combo);

    layout->addWidget(new QLabel(str_label("response"), this));
    response_combo = new QComboBox(this);
    response_combo->setEditable(true);
    response_combo->setObjectName(
        QStringLiteral("settings_active_line_response_combo")
    );
    line_profile_panel_support::populate_combo(
        response_combo, suggested_line_response_texts()
    );
    response_combo->setCurrentText(default_line_response_text());
    layout->addWidget(response_combo);

    summary_label = new QLabel(this);
    summary_label->setObjectName(QStringLiteral("settings_active_line_summary_label"));
    summary_label->setWordWrap(true);
    layout->addWidget(summary_label);

    undo_button = new QPushButton(str_label("undo point"), this);
    undo_button->setObjectName(QStringLiteral("settings_active_line_undo_button"));
    layout->addWidget(undo_button);

    save_button = new QPushButton(str_label("add line"), this);
    save_button->setObjectName(QStringLiteral("settings_active_line_save_button"));
    layout->addWidget(save_button);

    connect(
        color_button, &QPushButton::clicked, this,
        &line_profile_panel::on_color_clicked
    );
    connect(
        undo_button, &QPushButton::clicked, this,
        &line_profile_panel::on_undo_clicked
    );
    connect(
        save_button, &QPushButton::clicked, this,
        &line_profile_panel::on_save_clicked
    );
    connect(
        name_edit, &QLineEdit::editingFinished, this,
        &line_profile_panel::on_name_finished
    );
    connect(
        width_combo, &QComboBox::currentTextChanged, this,
        &line_profile_panel::on_width_changed
    );
    connect(
        length_combo, &QComboBox::currentTextChanged, this,
        &line_profile_panel::on_length_changed
    );
    connect(
        response_combo, &QComboBox::currentTextChanged, this,
        &line_profile_panel::on_response_changed
    );
    connect(
        closed_checkbox, &QCheckBox::toggled, this,
        &line_profile_panel::on_closed_toggled
    );

    setLayout(layout);
}

void line_profile_panel::refresh_summary() {
    if (summary_label == nullptr) {
        return;
    }

    const line_profile profile = current_line_profile();
    QString text = line_profile_summary_text(
        profile.width_text, profile.length_text, profile.response_text
    );
    text += profile.closed ? QStringLiteral(" closed") : QStringLiteral(" open");

    if (!profile.name.isEmpty()) {
        text = QStringLiteral("%1: %2").arg(profile.name, text);
    }

    summary_label->setText(text);
}
