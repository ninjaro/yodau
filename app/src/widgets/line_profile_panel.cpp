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
        || color_button == nullptr || color_mode_combo == nullptr
        || advanced_settings_checkbox == nullptr || width_combo == nullptr
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
    {
        QSignalBlocker blocker(color_mode_combo);
        const QString color_mode_id
            = normalized_line_color_mode_id(profile.color_mode_id);
        const int color_mode_index = color_mode_combo->findData(color_mode_id);
        color_mode_combo->setCurrentIndex(
            color_mode_index >= 0 ? color_mode_index : 0
        );
    }
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

    refresh_color_controls();
    refresh_advanced_settings_controls();
    refresh_parameter_mode_labels();
    refresh_summary();
}

line_profile line_profile_panel::current_line_profile() const {
    line_profile profile;

    if (name_edit != nullptr) {
        profile.name = name_edit->text().trimmed();
    }

    profile.color = current_color;
    profile.color_mode_id = current_color_mode_id();
    profile.closed = closed_checkbox != nullptr && closed_checkbox->isChecked();
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
    if (current_color_mode_id() != QStringLiteral("manual")) {
        return;
    }

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

void line_profile_panel::on_random_color_clicked() {
    if (current_color_mode_id() != QStringLiteral("manual")) {
        return;
    }

    current_color = random_manual_line_color();
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

void line_profile_panel::on_color_mode_changed(const int index) {
    Q_UNUSED(index);

    refresh_color_controls();
    refresh_summary();
    emit profile_changed(current_line_profile());
}

void line_profile_panel::on_advanced_settings_toggled(const bool checked) {
    Q_UNUSED(checked);

    refresh_advanced_settings_controls();
    refresh_parameter_mode_labels();
    refresh_summary();
    emit profile_changed(current_line_profile());
}

void line_profile_panel::on_parameter_mode_changed(const int index) {
    Q_UNUSED(index);

    refresh_parameter_mode_labels();
    refresh_summary();
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

    color_mode_combo = new QComboBox(this);
    color_mode_combo->setObjectName(
        QStringLiteral("active_line_color_mode_combo")
    );
    for (const QString& mode_id : line_color_mode_ids()) {
        color_mode_combo->addItem(
            line_color_mode_display_name(mode_id), mode_id
        );
    }
    layout->addWidget(color_mode_combo);

    color_button = new QPushButton(str_label("color"), this);
    color_button->setObjectName(
        QStringLiteral("settings_active_line_color_button")
    );
    line_profile_panel_support::set_button_color(color_button, current_color);
    layout->addWidget(color_button);

    random_color_button = new QPushButton(str_label("random color"), this);
    random_color_button->setObjectName(
        QStringLiteral("active_line_random_color_button")
    );
    layout->addWidget(random_color_button);

    advanced_settings_checkbox
        = new QCheckBox(str_label("advanced settings"), this);
    advanced_settings_checkbox->setObjectName(
        QStringLiteral("settings_active_line_advanced_checkbox")
    );
    advanced_settings_checkbox->setChecked(false);
    layout->addWidget(advanced_settings_checkbox);

    parameter_mode_combo = new QComboBox(this);
    parameter_mode_combo->setObjectName(
        QStringLiteral("active_line_parameter_mode_combo")
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
        QStringLiteral("active_line_parameter_hint_label")
    );
    parameter_mode_hint_label->setWordWrap(true);
    layout->addWidget(parameter_mode_hint_label);

    width_label = new QLabel(this);
    layout->addWidget(width_label);
    width_combo = new QComboBox(this);
    width_combo->setEditable(true);
    width_combo->setObjectName(
        QStringLiteral("settings_active_line_width_combo")
    );
    line_profile_panel_support::populate_combo(
        width_combo, suggested_line_width_texts()
    );
    width_combo->setCurrentText(default_line_width_text());
    layout->addWidget(width_combo);

    length_label = new QLabel(this);
    layout->addWidget(length_label);
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

    response_label = new QLabel(this);
    layout->addWidget(response_label);
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
    summary_label->setObjectName(
        QStringLiteral("settings_active_line_summary_label")
    );
    summary_label->setWordWrap(true);
    layout->addWidget(summary_label);

    undo_button = new QPushButton(str_label("undo point"), this);
    undo_button->setObjectName(
        QStringLiteral("settings_active_line_undo_button")
    );
    layout->addWidget(undo_button);

    save_button = new QPushButton(str_label("add line"), this);
    save_button->setObjectName(
        QStringLiteral("settings_active_line_save_button")
    );
    layout->addWidget(save_button);

    connect(
        color_button, &QPushButton::clicked, this,
        &line_profile_panel::on_color_clicked
    );
    connect(
        random_color_button, &QPushButton::clicked, this,
        &line_profile_panel::on_random_color_clicked
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
        color_mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &line_profile_panel::on_color_mode_changed
    );
    connect(
        advanced_settings_checkbox, &QCheckBox::toggled, this,
        &line_profile_panel::on_advanced_settings_toggled
    );
    connect(
        parameter_mode_combo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &line_profile_panel::on_parameter_mode_changed
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
    refresh_color_controls();
    refresh_advanced_settings_controls();
    refresh_parameter_mode_labels();
}

QString line_profile_panel::current_color_mode_id() const {
    return color_mode_combo != nullptr
        ? normalized_line_color_mode_id(
              color_mode_combo->currentData().toString()
          )
        : default_line_color_mode_id();
}

QString line_profile_panel::current_parameter_mode_id() const {
    if (!advanced_settings_enabled()) {
        return default_line_parameter_mode_id();
    }

    return parameter_mode_combo != nullptr
        ? normalized_line_parameter_mode_id(
              parameter_mode_combo->currentData().toString()
          )
        : default_line_parameter_mode_id();
}

bool line_profile_panel::advanced_settings_enabled() const {
    return advanced_settings_checkbox != nullptr
        && advanced_settings_checkbox->isChecked();
}

void line_profile_panel::refresh_color_controls() {
    const bool manual_color
        = current_color_mode_id() == QStringLiteral("manual");

    if (color_button != nullptr) {
        color_button->setVisible(manual_color);
        color_button->setEnabled(manual_color);
    }
    if (random_color_button != nullptr) {
        random_color_button->setVisible(manual_color);
        random_color_button->setEnabled(manual_color);
    }
}

void line_profile_panel::refresh_advanced_settings_controls() {
    const bool advanced_enabled = advanced_settings_enabled();

    if (parameter_mode_combo != nullptr) {
        parameter_mode_combo->setVisible(advanced_enabled);
        parameter_mode_combo->setEnabled(advanced_enabled);
    }
    if (parameter_mode_hint_label != nullptr) {
        parameter_mode_hint_label->setVisible(advanced_enabled);
    }
    if (width_label != nullptr) {
        width_label->setVisible(advanced_enabled);
    }
    if (width_combo != nullptr) {
        width_combo->setVisible(advanced_enabled);
        width_combo->setEnabled(advanced_enabled);
    }
    if (length_label != nullptr) {
        length_label->setVisible(advanced_enabled);
    }
    if (length_combo != nullptr) {
        length_combo->setVisible(advanced_enabled);
        length_combo->setEnabled(advanced_enabled);
    }
    if (response_label != nullptr) {
        response_label->setVisible(advanced_enabled);
    }
    if (response_combo != nullptr) {
        response_combo->setVisible(advanced_enabled);
        response_combo->setEnabled(advanced_enabled);
    }
}

void line_profile_panel::refresh_parameter_mode_labels() {
    const QString mode_id = current_parameter_mode_id();

    if (width_label != nullptr) {
        width_label->setText(
            str_label("%1").arg(line_width_label_text(mode_id))
        );
    }
    if (length_label != nullptr) {
        length_label->setText(
            str_label("%1").arg(line_length_label_text(mode_id))
        );
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

void line_profile_panel::refresh_summary() {
    if (summary_label == nullptr) {
        return;
    }

    const line_profile profile = current_line_profile();
    QString text = line_profile_summary_text(
        profile.width_text, profile.length_text, profile.response_text,
        current_parameter_mode_id()
    );
    text
        += profile.closed ? QStringLiteral(" closed") : QStringLiteral(" open");
    text += QStringLiteral(" color=%1")
                .arg(line_color_mode_display_name(profile.color_mode_id));
    if (profile.color_mode_id == QStringLiteral("manual")) {
        text += QStringLiteral(" %1").arg(profile.color.name(QColor::HexRgb));
    }

    if (!profile.name.isEmpty()) {
        text = QStringLiteral("%1: %2").arg(profile.name, text);
    }

    summary_label->setText(text);
}
