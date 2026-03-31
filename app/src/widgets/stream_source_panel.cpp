#include "widgets/stream_source_panel.hpp"

#include "shell/str_label.hpp"
#include "widgets/log_area_view.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace stream_source_panel_support {

frontend_log_entry make_log_entry(
    const frontend_log_severity severity, const QString& message,
    const QString& stream_name = QString(), const QString& detail = QString()
) {
    frontend_log_entry entry;
    entry.area = frontend_log_area::add;
    entry.severity = severity;
    entry.subsystem = QStringLiteral("stream_source_panel");
    entry.stream_name = stream_name;
    entry.message = message;
    entry.detail = detail;
    return entry;
}

} // namespace stream_source_panel_support

stream_source_panel::stream_source_panel(QWidget* parent)
    : QWidget(parent) {
    build_ui();
    set_mode(input_mode::file);
    update_add_enabled();
}

void stream_source_panel::set_existing_names(QSet<QString> names) {
    existing_names = std::move(names);
    on_name_changed(name_edit != nullptr ? name_edit->text() : QString());
}

void stream_source_panel::add_existing_name(const QString& name) {
    if (name.isEmpty()) {
        return;
    }

    existing_names.insert(name);
    on_name_changed(name_edit != nullptr ? name_edit->text() : QString());
}

void stream_source_panel::remove_existing_name(const QString& name) {
    existing_names.remove(name);
    on_name_changed(name_edit != nullptr ? name_edit->text() : QString());
}

void stream_source_panel::set_local_sources(const QStringList& sources) const {
    if (local_sources_combo == nullptr) {
        return;
    }

    local_sources_combo->clear();
    local_sources_combo->addItems(sources);
    if (!sources.isEmpty()) {
        local_sources_combo->setCurrentIndex(0);
    }
    update_add_enabled();
}

void stream_source_panel::clear_inputs() const {
    if (name_edit == nullptr || file_path_edit == nullptr || url_edit == nullptr
        || local_sources_combo == nullptr) {
        return;
    }

    name_edit->clear();
    file_path_edit->clear();
    url_edit->clear();
    local_sources_combo->setCurrentIndex(-1);
    set_name_error(false);
    update_add_enabled();
}

void stream_source_panel::set_log_toolbar(log_toolbar_panel* toolbar) {
    if (add_log_view != nullptr) {
        add_log_view->set_log_toolbar(toolbar);
    }
}

bool stream_source_panel::append_log_entry(
    const frontend_log_entry& entry
) const {
    return add_log_view != nullptr && add_log_view->append_entry(entry);
}

void stream_source_panel::build_ui() {
    const auto layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    const auto name_box = new QGroupBox(str_label("name (optional)"), this);
    const auto name_layout = new QVBoxLayout(name_box);
    name_edit = new QLineEdit(name_box);
    name_edit->setObjectName(QStringLiteral("settings_add_name_edit"));
    name_layout->addWidget(name_edit);
    layout->addWidget(name_box);

    connect(
        name_edit, &QLineEdit::textChanged, this,
        &stream_source_panel::on_name_changed
    );

    const auto mode_box = new QGroupBox(str_label("source"), this);
    const auto mode_layout = new QHBoxLayout(mode_box);
    mode_group = new QButtonGroup(this);
    file_radio = new QRadioButton(str_label("file"), mode_box);
    file_radio->setObjectName(QStringLiteral("settings_add_file_radio"));
    local_radio = new QRadioButton(str_label("local"), mode_box);
    local_radio->setObjectName(QStringLiteral("settings_add_local_radio"));
    url_radio = new QRadioButton(str_label("url"), mode_box);
    url_radio->setObjectName(QStringLiteral("settings_add_url_radio"));

    mode_group->addButton(file_radio, static_cast<int>(input_mode::file));
    mode_group->addButton(local_radio, static_cast<int>(input_mode::local));
    mode_group->addButton(url_radio, static_cast<int>(input_mode::url));

    mode_layout->addWidget(file_radio);
    mode_layout->addWidget(local_radio);
    mode_layout->addWidget(url_radio);
    layout->addWidget(mode_box);

    connect(
        mode_group, &QButtonGroup::idClicked, this,
        &stream_source_panel::on_mode_group_clicked
    );

    add_file_box = new QGroupBox(str_label("file stream"), this);
    const auto file_layout = new QVBoxLayout(add_file_box);
    const auto file_form = new QFormLayout();
    file_path_edit = new QLineEdit(add_file_box);
    file_path_edit->setObjectName(QStringLiteral("settings_add_file_path_edit"));
    file_path_edit->setReadOnly(true);
    loop_checkbox = new QCheckBox(str_label("loop"), add_file_box);
    loop_checkbox->setObjectName(QStringLiteral("settings_add_loop_checkbox"));
    loop_checkbox->setChecked(true);
    file_form->addRow(str_label("path"), file_path_edit);
    file_form->addRow(QString(), loop_checkbox);
    file_layout->addLayout(file_form);

    const auto file_btn_row = new QHBoxLayout();
    choose_file_btn = new QPushButton(str_label("choose file"), add_file_box);
    choose_file_btn->setObjectName(
        QStringLiteral("settings_add_choose_file_button")
    );
    file_btn_row->addWidget(choose_file_btn);
    file_layout->addLayout(file_btn_row);
    layout->addWidget(add_file_box);

    connect(
        choose_file_btn, &QPushButton::clicked, this,
        &stream_source_panel::on_choose_file
    );

    add_local_box = new QGroupBox(str_label("local sources"), this);
    const auto local_layout = new QVBoxLayout(add_local_box);
    local_sources_combo = new QComboBox(add_local_box);
    local_sources_combo->setObjectName(
        QStringLiteral("settings_add_local_sources_combo")
    );
    local_layout->addWidget(local_sources_combo);
    refresh_local_btn = new QPushButton(str_label("refresh"), add_local_box);
    refresh_local_btn->setObjectName(
        QStringLiteral("settings_add_refresh_local_button")
    );
    local_layout->addWidget(refresh_local_btn);
    layout->addWidget(add_local_box);

    connect(
        refresh_local_btn, &QPushButton::clicked, this,
        &stream_source_panel::on_refresh_local
    );
    connect(
        local_sources_combo, &QComboBox::currentTextChanged, this,
        &stream_source_panel::on_local_source_changed
    );

    add_url_box = new QGroupBox(str_label("url stream"), this);
    const auto url_layout = new QVBoxLayout(add_url_box);
    const auto url_form = new QFormLayout();
    url_edit = new QLineEdit(add_url_box);
    url_edit->setObjectName(QStringLiteral("settings_add_url_edit"));
    url_form->addRow(str_label("url"), url_edit);
    url_layout->addLayout(url_form);
    layout->addWidget(add_url_box);

    connect(
        url_edit, &QLineEdit::textChanged, this,
        &stream_source_panel::on_url_text_changed
    );

    add_btn = new QPushButton(str_label("add"), this);
    add_btn->setObjectName(QStringLiteral("settings_add_button"));
    layout->addWidget(add_btn);

    connect(
        add_btn, &QPushButton::clicked, this, &stream_source_panel::on_add_clicked
    );

    add_log_view = new log_area_view(frontend_log_area::add, this);
    add_log_view->setObjectName(QStringLiteral("settings_add_log_view"));
    add_log_view->setMinimumHeight(120);
    layout->addWidget(add_log_view);
}

void stream_source_panel::set_mode(const input_mode mode) {
    current_mode = mode;

    if (file_radio != nullptr) {
        file_radio->setChecked(mode == input_mode::file);
    }
    if (local_radio != nullptr) {
        local_radio->setChecked(mode == input_mode::local);
    }
    if (url_radio != nullptr) {
        url_radio->setChecked(mode == input_mode::url);
    }

    if (file_path_edit != nullptr) {
        file_path_edit->setEnabled(mode == input_mode::file);
    }
    if (choose_file_btn != nullptr) {
        choose_file_btn->setEnabled(mode == input_mode::file);
    }
    if (loop_checkbox != nullptr) {
        loop_checkbox->setEnabled(mode == input_mode::file);
    }

    if (local_sources_combo != nullptr) {
        local_sources_combo->setEnabled(mode == input_mode::local);
    }
    if (refresh_local_btn != nullptr) {
        refresh_local_btn->setEnabled(mode == input_mode::local);
    }

    if (url_edit != nullptr) {
        url_edit->setEnabled(mode == input_mode::url);
    }

    update_tools();
    update_add_enabled();
}

void stream_source_panel::update_tools() const {
    if (add_file_box == nullptr || add_local_box == nullptr
        || add_url_box == nullptr) {
        return;
    }

    const bool file_on = current_mode == input_mode::file;
    const bool local_on = current_mode == input_mode::local;
    const bool url_on = current_mode == input_mode::url;

    add_file_box->setVisible(file_on);
    add_file_box->setEnabled(file_on);
    add_local_box->setVisible(local_on);
    add_local_box->setEnabled(local_on);
    add_url_box->setVisible(url_on);
    add_url_box->setEnabled(url_on);
}

void stream_source_panel::update_add_enabled() const {
    if (add_btn == nullptr) {
        return;
    }

    const QString name = resolved_name_for_current_input();
    add_btn->setEnabled(name_is_unique(name) && current_input_valid());
}

QString stream_source_panel::resolved_name_for_current_input() const {
    return name_edit != nullptr ? name_edit->text().trimmed() : QString();
}

bool stream_source_panel::name_is_unique(const QString& name) const {
    if (name.isEmpty()) {
        return true;
    }
    if (name.compare(str_label("none"), Qt::CaseInsensitive) == 0) {
        return false;
    }
    return !existing_names.contains(name);
}

bool stream_source_panel::current_input_valid() const {
    switch (current_mode) {
    case input_mode::file:
        return file_path_edit != nullptr
            && !file_path_edit->text().trimmed().isEmpty();
    case input_mode::local:
        return local_sources_combo != nullptr
            && !local_sources_combo->currentText().trimmed().isEmpty();
    case input_mode::url:
        return url_edit != nullptr && !url_edit->text().trimmed().isEmpty();
    }

    return false;
}

void stream_source_panel::set_name_error(const bool error) const {
    if (name_edit == nullptr) {
        return;
    }

    if (!error) {
        name_edit->setStyleSheet(QString());
        name_edit->setToolTip(QString());
        return;
    }

    name_edit->setStyleSheet("border: 1px solid red;");
    name_edit->setToolTip(str_label("name is already taken"));
}

void stream_source_panel::on_choose_file() {
    const QString filters = str_label(
        "Video files (*.mp4 *.mkv *.avi *.mov *.webm *.m4v);;All files (*)"
    );
    const QString path = QFileDialog::getOpenFileName(
        this, str_label("choose video"), QString(), filters
    );
    if (!path.isEmpty() && file_path_edit != nullptr) {
        file_path_edit->setText(path);
        emit log_requested(stream_source_panel_support::make_log_entry(
            frontend_log_severity::info, QStringLiteral("file selected"),
            QString(), path
        ));
    }
    update_add_enabled();
}

void stream_source_panel::on_add_clicked() {
    const QString name = resolved_name_for_current_input();
    if (!name_is_unique(name)) {
        emit log_requested(stream_source_panel_support::make_log_entry(
            frontend_log_severity::warning,
            QStringLiteral("stream name already exists"), name
        ));
        set_name_error(true);
        update_add_enabled();
        return;
    }

    if (!current_input_valid()) {
        emit log_requested(stream_source_panel_support::make_log_entry(
            frontend_log_severity::warning,
            QStringLiteral("stream add input is incomplete"), name
        ));
        update_add_enabled();
        return;
    }

    switch (current_mode) {
    case input_mode::file: {
        const QString path
            = file_path_edit != nullptr ? file_path_edit->text().trimmed()
                                        : QString();
        const bool loop = loop_checkbox != nullptr && loop_checkbox->isChecked();
        emit log_requested(stream_source_panel_support::make_log_entry(
            frontend_log_severity::info,
            QStringLiteral("requested file stream add"), name,
            QStringLiteral("path=%1 loop=%2")
                .arg(
                    path,
                    loop ? QStringLiteral("true") : QStringLiteral("false")
                )
        ));
        emit add_file_stream(path, name, loop);
        break;
    }
    case input_mode::local: {
        const QString source = local_sources_combo != nullptr
            ? local_sources_combo->currentText().trimmed()
            : QString();
        emit log_requested(stream_source_panel_support::make_log_entry(
            frontend_log_severity::info,
            QStringLiteral("requested local stream add"), name, source
        ));
        emit add_local_stream(source, name);
        break;
    }
    case input_mode::url: {
        const QString url
            = url_edit != nullptr ? url_edit->text().trimmed() : QString();
        emit log_requested(stream_source_panel_support::make_log_entry(
            frontend_log_severity::info,
            QStringLiteral("requested url stream add"), name, url
        ));
        emit add_url_stream(url, name);
        break;
    }
    }
}

void stream_source_panel::on_refresh_local() {
    emit detect_local_sources_requested();
    emit log_requested(stream_source_panel_support::make_log_entry(
        frontend_log_severity::info,
        QStringLiteral("local source detection requested")
    ));
}

void stream_source_panel::on_name_changed(QString) const {
    const QString name = resolved_name_for_current_input();
    set_name_error(!name_is_unique(name));
    update_add_enabled();
}

void stream_source_panel::on_mode_group_clicked(const int id) {
    set_mode(static_cast<input_mode>(id));
}

void stream_source_panel::on_local_source_changed() { update_add_enabled(); }

void stream_source_panel::on_url_text_changed() { update_add_enabled(); }
