#include "widgets/settings_panel.hpp"
#include "helpers/str_label.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTreeWidget>

settings_panel::settings_panel(QWidget* parent)
    : QWidget(parent)
    , tabs(new QTabWidget(this))
    , add_tab(nullptr)
    , name_edit(nullptr)
    , mode_group(new QButtonGroup(this))
    , file_radio(nullptr)
    , local_radio(nullptr)
    , url_radio(nullptr)
    , current_mode(input_mode::file)
    , file_path_edit(nullptr)
    , choose_file_btn(nullptr)
    , loop_checkbox(nullptr)
    , local_sources_combo(nullptr)
    , refresh_local_btn(nullptr)
    , url_edit(nullptr)
    , add_btn(nullptr)
    , add_log_view(nullptr)
    , streams_tab(nullptr)
    , streams_list(nullptr)
    , event_log_view(nullptr) {

    build_ui();
    set_mode(input_mode::file);
    update_add_enabled();
}

void settings_panel::set_existing_names(QSet<QString> names) {
    existing_names = std::move(names);
    on_name_changed(name_edit->text());
}

void settings_panel::add_existing_name(const QString& name) {
    if (name.isEmpty()) {
        return;
    }
    existing_names.insert(name);
    on_name_changed(name_edit->text());
}

void settings_panel::remove_existing_name(const QString& name) {
    existing_names.remove(name);
    on_name_changed(name_edit->text());
}

void settings_panel::add_stream_entry(
    const QString& name, const QString& source, const bool checked
) const {
    QSignalBlocker blocker(streams_list);
    for (int i = 0; i < streams_list->topLevelItemCount(); ++i) {
        const auto item = streams_list->topLevelItem(i);
        if (item->text(1) == name) {
            return;
        }
    }

    const auto item = new QTreeWidgetItem(streams_list);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
    item->setText(1, name);
    item->setText(2, source);
    streams_list->addTopLevelItem(item);
}

void settings_panel::set_stream_checked(
    const QString& name, const bool checked
) const {
    QSignalBlocker blocker(streams_list);

    for (int i = 0; i < streams_list->topLevelItemCount(); ++i) {
        const auto item = streams_list->topLevelItem(i);
        if (item->text(1) == name) {
            item->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
            break;
        }
    }
}

void settings_panel::remove_stream_entry(const QString& name) const {
    for (int i = 0; i < streams_list->topLevelItemCount(); ++i) {
        const auto item = streams_list->topLevelItem(i);
        if (item->text(1) == name) {
            delete streams_list->takeTopLevelItem(i);
            break;
        }
    }
}

void settings_panel::clear_stream_entries() {
    streams_list->clear();
    existing_names.clear();
    update_add_enabled();
}

void settings_panel::append_event(QString text) const {
    auto ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    event_log_view->appendPlainText(QString("[%1] %2").arg(ts, text));
}

void settings_panel::append_add_log(const QString& text) const {
    add_log_view->appendPlainText(text);
}

void settings_panel::set_local_sources(const QStringList& sources) const {
    local_sources_combo->clear();
    local_sources_combo->addItems(sources);
    if (!sources.isEmpty()) {
        local_sources_combo->setCurrentIndex(0);
    }
    update_add_enabled();
}

void settings_panel::clear_add_inputs() const {
    name_edit->clear();
    file_path_edit->clear();
    url_edit->clear();
    local_sources_combo->setCurrentIndex(-1);
    set_name_error(false);
    update_add_enabled();
}

void settings_panel::build_ui() {
    const auto root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(8, 8, 8, 8);
    root_layout->addWidget(tabs);
    setLayout(root_layout);

    add_tab = build_add_tab();
    streams_tab = build_streams_tab();

    tabs->addTab(add_tab, str_label("add stream"));
    tabs->addTab(streams_tab, str_label("streams"));
}

QWidget* settings_panel::build_add_tab() {
    const auto w = new QWidget(this);
    const auto layout = new QVBoxLayout(w);
    layout->setSpacing(10);

    const auto name_box = new QGroupBox(str_label("name"), w);
    const auto name_layout = new QVBoxLayout(name_box);
    name_edit = new QLineEdit(name_box);
    name_layout->addWidget(name_edit);
    name_box->setLayout(name_layout);
    layout->addWidget(name_box);

    connect(
        name_edit, &QLineEdit::textChanged, this,
        &settings_panel::on_name_changed
    );

    const auto mode_box = new QGroupBox(str_label("source"), w);
    const auto mode_layout = new QHBoxLayout(mode_box);
    file_radio = new QRadioButton(str_label("file"), mode_box);
    local_radio = new QRadioButton(str_label("local"), mode_box);
    url_radio = new QRadioButton(str_label("url"), mode_box);

    mode_group->addButton(file_radio, static_cast<int>(input_mode::file));
    mode_group->addButton(local_radio, static_cast<int>(input_mode::local));
    mode_group->addButton(url_radio, static_cast<int>(input_mode::url));

    mode_layout->addWidget(file_radio);
    mode_layout->addWidget(local_radio);
    mode_layout->addWidget(url_radio);
    mode_box->setLayout(mode_layout);
    layout->addWidget(mode_box);

    connect(mode_group, &QButtonGroup::idClicked, this, [this](int id) {
        set_mode(static_cast<input_mode>(id));
        update_add_enabled();
    });

    const auto file_box = new QGroupBox(str_label("file stream"), w);
    const auto file_layout = new QVBoxLayout(file_box);
    const auto file_form = new QFormLayout();
    file_path_edit = new QLineEdit(file_box);
    file_path_edit->setReadOnly(true);
    loop_checkbox = new QCheckBox(str_label("loop"), file_box);
    loop_checkbox->setChecked(true);
    file_form->addRow(str_label("path"), file_path_edit);
    file_form->addRow(QString(), loop_checkbox);
    file_layout->addLayout(file_form);

    const auto file_btn_row = new QHBoxLayout();
    choose_file_btn = new QPushButton(str_label("choose file"), file_box);
    file_btn_row->addWidget(choose_file_btn);
    file_layout->addLayout(file_btn_row);

    file_box->setLayout(file_layout);
    layout->addWidget(file_box);

    connect(
        choose_file_btn, &QPushButton::clicked, this,
        &settings_panel::on_choose_file
    );

    const auto local_box = new QGroupBox(str_label("local sources"), w);
    const auto local_layout = new QVBoxLayout(local_box);
    local_sources_combo = new QComboBox(local_box);
    local_sources_combo->setEditable(false);
    local_layout->addWidget(local_sources_combo);
    refresh_local_btn = new QPushButton(str_label("refresh"), local_box);
    local_layout->addWidget(refresh_local_btn);
    local_box->setLayout(local_layout);
    layout->addWidget(local_box);

    connect(
        refresh_local_btn, &QPushButton::clicked, this,
        &settings_panel::on_refresh_local
    );
    connect(
        local_sources_combo, &QComboBox::currentTextChanged, this,
        [this]() { update_add_enabled(); }
    );

    const auto url_box = new QGroupBox(str_label("url stream"), w);
    const auto url_layout = new QVBoxLayout(url_box);
    const auto url_form = new QFormLayout();
    url_edit = new QLineEdit(url_box);
    url_form->addRow(str_label("url"), url_edit);
    url_layout->addLayout(url_form);
    url_box->setLayout(url_layout);
    layout->addWidget(url_box);

    connect(url_edit, &QLineEdit::textChanged, this, [this]() {
        update_add_enabled();
    });

    add_btn = new QPushButton(str_label("add"), w);
    layout->addWidget(add_btn);

    connect(
        add_btn, &QPushButton::clicked, this, &settings_panel::on_add_clicked
    );

    add_log_view = new QPlainTextEdit(w);
    add_log_view->setReadOnly(true);
    add_log_view->setMinimumHeight(120);
    layout->addWidget(add_log_view);

    w->setLayout(layout);
    return w;
}

QWidget* settings_panel::build_streams_tab() {
    const auto w = new QWidget(this);
    const auto layout = new QVBoxLayout(w);
    layout->setSpacing(10);

    streams_list = new QTreeWidget(w);
    streams_list->setColumnCount(3);
    streams_list->setHeaderLabels(
        { str_label("show"), str_label("name"), str_label("source") }
    );
    streams_list->header()->setSectionResizeMode(
        0, QHeaderView::ResizeToContents
    );
    streams_list->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    streams_list->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    layout->addWidget(streams_list);

    connect(
        streams_list, &QTreeWidget::itemChanged, this,
        [this](const QTreeWidgetItem* item, const int column) {
            if (column != 0) {
                return;
            }
            auto name = item->text(1);
            const auto show = item->checkState(0) == Qt::Checked;
            emit show_stream_changed(name, show);
            auto ts = QDateTime::currentDateTime().toString("HH:mm:ss");
            append_event(QString("show in grid: %1 = %2")
                             .arg(name, show ? "true" : "false"));
        }
    );

    event_log_view = new QPlainTextEdit(w);
    event_log_view->setReadOnly(true);
    event_log_view->setMinimumHeight(160);
    layout->addWidget(event_log_view);

    w->setLayout(layout);
    return w;
}

void settings_panel::set_mode(const input_mode mode) {
    current_mode = mode;

    file_radio->setChecked(mode == input_mode::file);
    local_radio->setChecked(mode == input_mode::local);
    url_radio->setChecked(mode == input_mode::url);

    file_path_edit->setEnabled(mode == input_mode::file);
    choose_file_btn->setEnabled(mode == input_mode::file);
    loop_checkbox->setEnabled(mode == input_mode::file);

    local_sources_combo->setEnabled(mode == input_mode::local);
    refresh_local_btn->setEnabled(mode == input_mode::local);

    url_edit->setEnabled(mode == input_mode::url);
}

void settings_panel::update_add_enabled() const {
    const auto name = resolved_name_for_current_input();
    const auto unique = name_is_unique(name);
    const auto input_ok = current_input_valid();
    add_btn->setEnabled(unique && input_ok);
}

void settings_panel::on_choose_file() {
    const auto filters = str_label(
        "Video files (*.mp4 *.mkv *.avi *.mov *.webm *.m4v);;All files (*)"
    );
    auto path = QFileDialog::getOpenFileName(
        this, str_label("choose video"), QString(), filters
    );
    if (!path.isEmpty()) {
        file_path_edit->setText(path);
        auto ts = QDateTime::currentDateTime().toString("HH:mm:ss");
        append_add_log(QString("[%1] file selected: %2").arg(ts, path));
    }
    update_add_enabled();
}

void settings_panel::on_add_clicked() {
    auto ts = QDateTime::currentDateTime().toString("HH:mm:ss");

    const auto name = resolved_name_for_current_input();
    if (!name_is_unique(name)) {
        append_add_log(QString("[%1] error: name already exists").arg(ts));
        set_name_error(true);
        update_add_enabled();
        return;
    }

    if (!current_input_valid()) {
        append_add_log(QString("[%1] error: input is incomplete").arg(ts));
        update_add_enabled();
        return;
    }

    switch (current_mode) {
    case input_mode::file: {
        auto path = file_path_edit->text().trimmed();
        const auto loop = loop_checkbox->isChecked();
        append_add_log(QString("[%1] request add file: %2").arg(ts, path));
        emit add_file_stream(path, name, loop);
        break;
    }
    case input_mode::local: {
        auto source = local_sources_combo->currentText().trimmed();
        append_add_log(QString("[%1] request add local: %2").arg(ts, source));
        emit add_local_stream(source, name);
        break;
    }
    case input_mode::url: {
        auto url = url_edit->text().trimmed();
        append_add_log(QString("[%1] request add url: %2").arg(ts, url));
        emit add_url_stream(url, name);
        break;
    }
    }
}

void settings_panel::on_refresh_local() {
    emit detect_local_sources_requested();
    const auto ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    append_add_log(QString("[%1] detect local sources requested").arg(ts));
}

void settings_panel::on_name_changed(QString) const {
    const auto name = resolved_name_for_current_input();
    const auto unique = name_is_unique(name);
    set_name_error(!unique);
    update_add_enabled();
}

QString settings_panel::resolved_name_for_current_input() const {
    return name_edit->text().trimmed();
}

bool settings_panel::name_is_unique(const QString& name) const {
    if (name.isEmpty()) {
        return true;
    }
    return !existing_names.contains(name);
}

bool settings_panel::current_input_valid() const {
    switch (current_mode) {
    case input_mode::file: {
        return !file_path_edit->text().trimmed().isEmpty();
    }
    case input_mode::local: {
        return !local_sources_combo->currentText().trimmed().isEmpty();
    }
    case input_mode::url: {
        return !url_edit->text().trimmed().isEmpty();
    }
    }
    return false;
}

void settings_panel::set_name_error(const bool error) const {
    if (!error) {
        name_edit->setStyleSheet(QString());
        name_edit->setToolTip(QString());
        return;
    }
    name_edit->setStyleSheet("border: 1px solid red;");
    name_edit->setToolTip(str_label("name is already taken"));
}
