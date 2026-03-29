#include "widgets/settings_panel.hpp"
#include "shell/str_label.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace settings_panel_support {

frontend_log_entry make_log_entry(
    const frontend_log_area area, const frontend_log_severity severity,
    const QString& subsystem, const QString& message,
    const QString& stream_name = QString(), const QString& detail = QString(),
    const QString& algorithm_id = QString()
) {
    frontend_log_entry entry;
    entry.area = area;
    entry.severity = severity;
    entry.subsystem = subsystem;
    entry.stream_name = stream_name;
    entry.algorithm_id = algorithm_id;
    entry.message = message;
    entry.detail = detail;
    return entry;
}

QString log_filter_all_text() { return str_label("all"); }

void replace_filter_combo_items(
    QComboBox* combo, const QStringList& values, const QString& selected_value
) {
    if (combo == nullptr) {
        return;
    }

    QSignalBlocker blocker(combo);
    combo->clear();
    combo->addItem(log_filter_all_text(), QString());

    for (const QString& value : values) {
        combo->addItem(value, value);
    }

    const int selected_index
        = selected_value.isEmpty() ? 0 : combo->findData(selected_value);
    combo->setCurrentIndex(selected_index >= 0 ? selected_index : 0);
}

} // namespace settings_panel_support

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

void settings_panel::set_log_buffer(frontend_log_buffer* buffer) {
    if (shared_log_buffer == buffer) {
        rebuild_log_views();
        return;
    }

    if (shared_log_buffer != nullptr) {
        disconnect(shared_log_buffer, nullptr, this, nullptr);
    }

    shared_log_buffer = buffer;

    if (shared_log_buffer != nullptr) {
        connect(
            shared_log_buffer, &frontend_log_buffer::entry_appended, this,
            [this](const frontend_log_entry&) { rebuild_log_views(); }
        );
        connect(
            shared_log_buffer, &frontend_log_buffer::cleared, this,
            [this]() { rebuild_log_views(); }
        );
    }

    rebuild_log_views();
}

void settings_panel::set_log_mode(const frontend_log_mode mode) {
    current_log_mode = mode;

    if (log_mode_combo != nullptr) {
        QSignalBlocker blocker(log_mode_combo);
        log_mode_combo->setCurrentIndex(
            mode == frontend_log_mode::release ? 0 : 1
        );
    }

    rebuild_log_views();
}

frontend_log_mode settings_panel::log_mode() const { return current_log_mode; }

void settings_panel::append_log(frontend_log_entry entry) const {
    if (shared_log_buffer != nullptr) {
        shared_log_buffer->append(std::move(entry));
        return;
    }

    QPlainTextEdit* target_view = nullptr;

    switch (entry.area) {
    case frontend_log_area::add:
        target_view = add_log_view;
        break;
    case frontend_log_area::streams:
        target_view = event_log_view;
        break;
    case frontend_log_area::active:
        target_view = active_log_view;
        break;
    }

    if (target_view == nullptr) {
        return;
    }

    if (!entry_matches_log_filters(entry)) {
        return;
    }

    target_view->appendPlainText(
        format_frontend_log_entry(current_log_mode, entry)
    );
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

    const auto item = new QTreeWidgetItem();
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
    item->setText(1, name);
    item->setText(2, source);
    streams_list->addTopLevelItem(item);
}

void settings_panel::set_stream_checked(
    const QString& name, const bool checked
) const {
    // QSignalBlocker blocker(streams_list);

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

void settings_panel::append_event(const QString& text) const {
    append_log(
        settings_panel_support::make_log_entry(
            frontend_log_area::streams, frontend_log_severity::info,
            QStringLiteral("settings_panel"), text
        )
    );
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

void settings_panel::append_add_log(const QString& text) const {
    append_log(
        settings_panel_support::make_log_entry(
            frontend_log_area::add, frontend_log_severity::info,
            QStringLiteral("settings_panel"), text
        )
    );
}

void settings_panel::set_active_candidates(const QStringList& names) const {
    if (!active_combo) {
        return;
    }

    const QString none_text = str_label("none");
    const QString current = (active_combo->currentText() == none_text)
        ? QString()
        : active_combo->currentText();

    QStringList final_names = names;

    if (!current.isEmpty() && !final_names.contains(current)) {
        final_names.prepend(current);
    }

    active_combo->blockSignals(true);
    active_combo->clear();
    active_combo->addItem(none_text, QVariant());

    for (const auto& n : final_names) {
        if (n.isEmpty() || n == none_text) {
            continue;
        }
        active_combo->addItem(n);
    }

    if (!current.isEmpty() && final_names.contains(current)) {
        active_combo->setCurrentText(current);
    } else {
        active_combo->setCurrentText(none_text);
    }

    active_combo->blockSignals(false);
    update_active_tools();
}

void settings_panel::set_active_current(const QString& name) const {
    if (!active_combo) {
        return;
    }

    active_combo->blockSignals(true);
    const auto none_text = str_label("none");
    if (name.isEmpty() || active_combo->findText(name) < 0) {
        active_combo->setCurrentText(none_text);
    } else {
        active_combo->setCurrentText(name);
    }
    active_combo->blockSignals(false);

    update_active_tools();
}

void settings_panel::add_template_candidate(const QString& name) const {
    if (!active_template_combo || name.isEmpty()) {
        return;
    }

    for (int i = 0; i < active_template_combo->count(); ++i) {
        if (active_template_combo->itemText(i) == name) {
            update_active_tools();
            return;
        }
    }

    active_template_combo->addItem(name);

    if (active_template_combo->count() == 1) {
        active_template_combo->setCurrentIndex(0);
    }

    update_active_tools();
}

void settings_panel::set_template_candidates(const QStringList& names) const {
    if (!active_template_combo) {
        return;
    }

    const QString none_text = str_label("none");

    active_template_combo->blockSignals(true);

    active_template_combo->clear();
    active_template_combo->addItem(none_text, QVariant());

    QSet<QString> seen;
    for (const auto& n : names) {
        const auto t = n.trimmed();
        if (t.isEmpty()) {
            continue;
        }
        if (t == none_text) {
            continue;
        }
        if (seen.contains(t)) {
            continue;
        }
        seen.insert(t);
        active_template_combo->addItem(t);
    }

    active_template_combo->setCurrentIndex(0);

    active_template_combo->blockSignals(false);

    update_active_tools();
}

void settings_panel::reset_active_line_form() {
    if (!active_line_name_edit || !active_line_closed_cb
        || !active_line_color_btn) {
        return;
    }

    active_line_name_edit->clear();

    {
        QSignalBlocker b(active_line_closed_cb);
        active_line_closed_cb->setChecked(false);
    }

    active_line_color = Qt::red;
    active_line_color_btn->setStyleSheet(
        QString("background-color: %1;").arg(active_line_color.name())
    );

    emit active_line_params_changed(QString(), active_line_color, false);
}

void settings_panel::reset_active_template_form() {
    if (!active_template_combo) {
        return;
    }
    const int idx = active_template_combo->findText(str_label("none"));
    if (idx >= 0) {
        active_template_combo->setCurrentIndex(idx);
    } else {
        active_template_combo->setCurrentIndex(-1);
    }
}

void settings_panel::set_active_line_closed(bool closed) const {
    if (!active_line_closed_cb) {
        return;
    }
    QSignalBlocker b(active_line_closed_cb);
    active_line_closed_cb->setChecked(closed);
}

QString settings_panel::active_template_current() const {
    if (!active_template_combo) {
        return {};
    }
    return active_template_combo->currentText().trimmed();
}

QColor settings_panel::active_template_preview_color() const {
    return active_template_color;
}

void settings_panel::append_active_log(const QString& msg) const {
    append_log(
        settings_panel_support::make_log_entry(
            frontend_log_area::active, frontend_log_severity::info,
            QStringLiteral("settings_panel"), msg
        )
    );
}

void settings_panel::clear_active_log() const {
    if (!active_log_view) {
        return;
    }
    active_log_view->clear();
}

void settings_panel::build_ui() {
    const auto root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(8, 8, 8, 8);
    root_layout->addWidget(build_log_toolbar());
    root_layout->addWidget(tabs, 1);
    // setLayout(root_layout);

    add_tab = build_add_tab();
    streams_tab = build_streams_tab();
    active_tab = build_active_tab();

    tabs->addTab(add_tab, str_label("add stream"));
    tabs->addTab(streams_tab, str_label("streams"));
    tabs->addTab(active_tab, str_label("active"));
}

QWidget* settings_panel::build_log_toolbar() {
    const auto toolbar = new QWidget(this);
    const auto layout = new QHBoxLayout(toolbar);
    layout->setContentsMargins(0, 0, 0, 0);

    const auto label = new QLabel(str_label("log mode"), toolbar);
    log_mode_combo = new QComboBox(toolbar);
    log_mode_combo->setObjectName(QStringLiteral("settings_log_mode_combo"));
    log_mode_combo->addItem(str_label("release"));
    log_mode_combo->addItem(str_label("debug"));
    log_mode_combo->setCurrentIndex(
        current_log_mode == frontend_log_mode::release ? 0 : 1
    );

    const auto severity_label = new QLabel(str_label("severity"), toolbar);
    log_severity_filter_combo = new QComboBox(toolbar);
    log_severity_filter_combo->setObjectName(
        QStringLiteral("settings_log_severity_filter_combo")
    );
    log_severity_filter_combo->addItem(
        settings_panel_support::log_filter_all_text(), -1
    );
    log_severity_filter_combo->addItem(
        frontend_log_severity_name(frontend_log_severity::debug),
        static_cast<int>(frontend_log_severity::debug)
    );
    log_severity_filter_combo->addItem(
        frontend_log_severity_name(frontend_log_severity::info),
        static_cast<int>(frontend_log_severity::info)
    );
    log_severity_filter_combo->addItem(
        frontend_log_severity_name(frontend_log_severity::warning),
        static_cast<int>(frontend_log_severity::warning)
    );
    log_severity_filter_combo->addItem(
        frontend_log_severity_name(frontend_log_severity::error),
        static_cast<int>(frontend_log_severity::error)
    );

    const auto stream_label = new QLabel(str_label("stream"), toolbar);
    log_stream_filter_combo = new QComboBox(toolbar);
    log_stream_filter_combo->setObjectName(
        QStringLiteral("settings_log_stream_filter_combo")
    );

    const auto subsystem_label = new QLabel(str_label("subsystem"), toolbar);
    log_subsystem_filter_combo = new QComboBox(toolbar);
    log_subsystem_filter_combo->setObjectName(
        QStringLiteral("settings_log_subsystem_filter_combo")
    );

    layout->addWidget(label);
    layout->addWidget(log_mode_combo);
    layout->addWidget(severity_label);
    layout->addWidget(log_severity_filter_combo);
    layout->addWidget(stream_label);
    layout->addWidget(log_stream_filter_combo);
    layout->addWidget(subsystem_label);
    layout->addWidget(log_subsystem_filter_combo);
    layout->addStretch(1);

    connect(
        log_mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &settings_panel::on_log_mode_changed
    );
    connect(
        log_severity_filter_combo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &settings_panel::on_log_severity_filter_changed
    );
    connect(
        log_stream_filter_combo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &settings_panel::on_log_stream_filter_changed
    );
    connect(
        log_subsystem_filter_combo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &settings_panel::on_log_subsystem_filter_changed
    );

    refresh_log_filter_options();

    toolbar->setLayout(layout);
    return toolbar;
}

QWidget* settings_panel::build_add_tab() {
    const auto w = new QWidget(this);
    const auto layout = new QVBoxLayout(w);
    layout->setSpacing(10);

    const auto name_box = new QGroupBox(str_label("name (optional)"), w);
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

    connect(
        mode_group, &QButtonGroup::idClicked, this,
        &settings_panel::on_mode_group_clicked
    );

    add_file_box = new QGroupBox(str_label("file stream"), w);
    const auto file_layout = new QVBoxLayout(add_file_box);
    const auto file_form = new QFormLayout();
    file_path_edit = new QLineEdit(add_file_box);
    file_path_edit->setReadOnly(true);
    loop_checkbox = new QCheckBox(str_label("loop"), add_file_box);
    loop_checkbox->setChecked(true);
    file_form->addRow(str_label("path"), file_path_edit);
    file_form->addRow(QString(), loop_checkbox);
    file_layout->addLayout(file_form);

    const auto file_btn_row = new QHBoxLayout();
    choose_file_btn = new QPushButton(str_label("choose file"), add_file_box);
    file_btn_row->addWidget(choose_file_btn);
    file_layout->addLayout(file_btn_row);

    add_file_box->setLayout(file_layout);
    layout->addWidget(add_file_box);

    connect(
        choose_file_btn, &QPushButton::clicked, this,
        &settings_panel::on_choose_file
    );

    add_local_box = new QGroupBox(str_label("local sources"), w);
    const auto local_layout = new QVBoxLayout(add_local_box);
    local_sources_combo = new QComboBox(add_local_box);
    local_sources_combo->setEditable(false);
    local_layout->addWidget(local_sources_combo);
    refresh_local_btn = new QPushButton(str_label("refresh"), add_local_box);
    local_layout->addWidget(refresh_local_btn);
    add_local_box->setLayout(local_layout);
    layout->addWidget(add_local_box);

    connect(
        refresh_local_btn, &QPushButton::clicked, this,
        &settings_panel::on_refresh_local
    );
    connect(
        local_sources_combo, &QComboBox::currentTextChanged, this,
        &settings_panel::on_local_source_changed
    );

    add_url_box = new QGroupBox(str_label("url stream"), w);
    const auto url_layout = new QVBoxLayout(add_url_box);
    const auto url_form = new QFormLayout();
    url_edit = new QLineEdit(add_url_box);
    url_form->addRow(str_label("url"), url_edit);
    url_layout->addLayout(url_form);
    add_url_box->setLayout(url_layout);
    layout->addWidget(add_url_box);

    connect(
        url_edit, &QLineEdit::textChanged, this,
        &settings_panel::on_url_text_changed
    );

    add_btn = new QPushButton(str_label("add"), w);
    layout->addWidget(add_btn);

    connect(
        add_btn, &QPushButton::clicked, this, &settings_panel::on_add_clicked
    );

    add_log_view = new QPlainTextEdit(w);
    add_log_view->setObjectName(QStringLiteral("settings_add_log_view"));
    add_log_view->setReadOnly(true);
    add_log_view->setMinimumHeight(120);
    layout->addWidget(add_log_view);

    w->setLayout(layout);
    update_add_tools();
    update_add_enabled();
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
        &settings_panel::on_stream_item_changed
    );

    event_log_view = new QPlainTextEdit(w);
    event_log_view->setObjectName(QStringLiteral("settings_streams_log_view"));
    event_log_view->setReadOnly(true);
    event_log_view->setMinimumHeight(160);
    layout->addWidget(event_log_view);

    w->setLayout(layout);
    return w;
}

QWidget* settings_panel::build_active_tab() {
    const auto w = new QWidget(this);
    const auto layout = new QVBoxLayout(w);
    layout->setSpacing(10);

    layout->addWidget(build_active_stream_box(w));
    layout->addWidget(build_edit_mode_box(w));
    layout->addWidget(build_new_line_box(w));
    layout->addWidget(build_templates_box(w));

    active_log_view = new QPlainTextEdit(w);
    active_log_view->setObjectName(QStringLiteral("settings_active_log_view"));
    active_log_view->setReadOnly(true);
    active_log_view->setMinimumHeight(160);
    active_log_view->setSizePolicy(
        QSizePolicy::Expanding, QSizePolicy::Expanding
    );
    layout->addWidget(active_log_view, 1);

    w->setLayout(layout);
    return w;
}

QWidget* settings_panel::build_active_stream_box(QWidget* parent) {
    const auto box = new QGroupBox(str_label("active stream"), parent);
    const auto box_layout = new QVBoxLayout(box);

    active_combo = new QComboBox(box);
    active_combo->setEditable(false);
    active_combo->addItem(str_label("none"), QVariant());

    box_layout->addWidget(active_combo);
    box->setLayout(box_layout);

    active_labels_cb = new QCheckBox(str_label("labels"), box);
    active_labels_cb->setChecked(true);
    box_layout->addWidget(active_labels_cb);

    connect(
        active_labels_cb, &QCheckBox::toggled, this,
        &settings_panel::active_labels_enabled_changed
    );

    connect(
        active_combo, &QComboBox::currentTextChanged, this,
        &settings_panel::on_active_combo_changed
    );

    return box;
}

QWidget* settings_panel::build_edit_mode_box(QWidget* parent) {
    active_mode_box = new QGroupBox(str_label("edit mode"), parent);
    const auto h = new QHBoxLayout(active_mode_box);

    active_mode_group = new QButtonGroup(active_mode_box);
    active_mode_draw_radio
        = new QRadioButton(str_label("draw new"), active_mode_box);
    active_mode_template_radio
        = new QRadioButton(str_label("use template"), active_mode_box);

    active_mode_group->addButton(active_mode_draw_radio, 0);
    active_mode_group->addButton(active_mode_template_radio, 1);

    active_mode_draw_radio->setChecked(true);

    h->addWidget(active_mode_draw_radio);
    h->addWidget(active_mode_template_radio);
    active_mode_box->setLayout(h);

    connect(
        active_mode_group, &QButtonGroup::idClicked, this,
        &settings_panel::on_active_mode_clicked
    );

    return active_mode_box;
}

QWidget* settings_panel::build_new_line_box(QWidget* parent) {
    active_line_box = new QGroupBox(str_label("new line"), parent);
    const auto v = new QVBoxLayout(active_line_box);

    active_line_name_edit = new QLineEdit(active_line_box);
    active_line_name_edit->setPlaceholderText(
        str_label("template name (optional)")
    );
    v->addWidget(active_line_name_edit);

    active_line_closed_cb = new QCheckBox(str_label("closed"), active_line_box);
    active_line_closed_cb->setChecked(false);
    v->addWidget(active_line_closed_cb);

    active_line_color_btn
        = new QPushButton(str_label("color"), active_line_box);
    set_btn_color(active_line_color_btn, active_line_color);
    v->addWidget(active_line_color_btn);

    active_line_undo_btn
        = new QPushButton(str_label("undo point"), active_line_box);
    v->addWidget(active_line_undo_btn);

    connect(
        active_line_undo_btn, &QPushButton::clicked, this,
        &settings_panel::on_active_line_undo_clicked
    );

    active_line_save_btn
        = new QPushButton(str_label("add line"), active_line_box);
    v->addWidget(active_line_save_btn);

    active_line_box->setLayout(v);

    connect(
        active_line_color_btn, &QPushButton::clicked, this,
        &settings_panel::on_active_line_color_clicked
    );

    connect(
        active_line_name_edit, &QLineEdit::editingFinished, this,
        &settings_panel::on_active_line_name_finished
    );

    connect(
        active_line_closed_cb, &QCheckBox::toggled, this,
        &settings_panel::on_active_line_closed_toggled
    );

    connect(
        active_line_save_btn, &QPushButton::clicked, this,
        &settings_panel::on_active_line_save_clicked
    );

    update_active_tools();
    return active_line_box;
}

QWidget* settings_panel::build_templates_box(QWidget* parent) {
    active_templates_box = new QGroupBox(str_label("templates"), parent);
    const auto v = new QVBoxLayout(active_templates_box);

    active_template_combo = new QComboBox(active_templates_box);
    active_template_combo->setEditable(false);
    active_template_combo->addItem(str_label("none"), QVariant());
    v->addWidget(active_template_combo);

    connect(
        active_template_combo, &QComboBox::currentTextChanged, this,
        &settings_panel::on_active_template_combo_changed
    );

    active_template_color_btn
        = new QPushButton(str_label("color"), active_templates_box);
    set_btn_color(active_template_color_btn, active_template_color);
    v->addWidget(active_template_color_btn);

    active_template_add_btn
        = new QPushButton(str_label("add template"), active_templates_box);
    v->addWidget(active_template_add_btn);

    active_templates_box->setLayout(v);

    connect(
        active_template_color_btn, &QPushButton::clicked, this,
        &settings_panel::on_active_template_color_clicked
    );

    connect(
        active_template_add_btn, &QPushButton::clicked, this,
        &settings_panel::on_active_template_add_clicked
    );

    update_active_tools();
    return active_templates_box;
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
    update_add_tools();
    update_add_enabled();
}

void settings_panel::on_mode_group_clicked(const int id) {
    set_mode(static_cast<input_mode>(id));
}

void settings_panel::on_local_source_changed() { update_add_enabled(); }

void settings_panel::on_url_text_changed() { update_add_enabled(); }

void settings_panel::update_add_tools() const {
    if (!add_file_box || !add_local_box || !add_url_box) {
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
        append_log(
            settings_panel_support::make_log_entry(
                frontend_log_area::add, frontend_log_severity::info,
                QStringLiteral("settings_panel"),
                QStringLiteral("file selected"), QString(), path
            )
        );
    }
    update_add_enabled();
}

void settings_panel::on_add_clicked() {
    const auto name = resolved_name_for_current_input();
    if (!name_is_unique(name)) {
        append_log(
            settings_panel_support::make_log_entry(
                frontend_log_area::add, frontend_log_severity::warning,
                QStringLiteral("settings_panel"),
                QStringLiteral("stream name already exists"), name
            )
        );
        set_name_error(true);
        update_add_enabled();
        return;
    }

    if (!current_input_valid()) {
        append_log(
            settings_panel_support::make_log_entry(
                frontend_log_area::add, frontend_log_severity::warning,
                QStringLiteral("settings_panel"),
                QStringLiteral("stream add input is incomplete"), name
            )
        );
        update_add_enabled();
        return;
    }

    switch (current_mode) {
    case input_mode::file: {
        const auto path = file_path_edit->text().trimmed();
        const auto loop = loop_checkbox->isChecked();
        append_log(
            settings_panel_support::make_log_entry(
                frontend_log_area::add, frontend_log_severity::info,
                QStringLiteral("settings_panel"),
                QStringLiteral("requested file stream add"), name,
                QStringLiteral("path=%1 loop=%2")
                    .arg(
                        path,
                        loop ? QStringLiteral("true") : QStringLiteral("false")
                    )
            )
        );
        emit add_file_stream(path, name, loop);
        break;
    }
    case input_mode::local: {
        const auto source = local_sources_combo->currentText().trimmed();
        append_log(
            settings_panel_support::make_log_entry(
                frontend_log_area::add, frontend_log_severity::info,
                QStringLiteral("settings_panel"),
                QStringLiteral("requested local stream add"), name, source
            )
        );
        emit add_local_stream(source, name);
        break;
    }
    case input_mode::url: {
        const auto url = url_edit->text().trimmed();
        append_log(
            settings_panel_support::make_log_entry(
                frontend_log_area::add, frontend_log_severity::info,
                QStringLiteral("settings_panel"),
                QStringLiteral("requested url stream add"), name, url
            )
        );
        emit add_url_stream(url, name);
        break;
    }
    }
}

void settings_panel::on_refresh_local() {
    emit detect_local_sources_requested();
    append_log(
        settings_panel_support::make_log_entry(
            frontend_log_area::add, frontend_log_severity::info,
            QStringLiteral("settings_panel"),
            QStringLiteral("local source detection requested")
        )
    );
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
    if (name.compare(str_label("none"), Qt::CaseInsensitive) == 0) {
        return false;
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

void settings_panel::update_active_tools() const {
    if (!active_combo) {
        return;
    }

    const bool has_active = active_combo->currentText() != str_label("none");
    const bool drawing_mode
        = active_mode_draw_radio && active_mode_draw_radio->isChecked();

    if (active_mode_box) {
        active_mode_box->setVisible(has_active);
        active_mode_box->setEnabled(has_active);
    }

    if (active_line_box) {
        const bool show_line = has_active && drawing_mode;
        active_line_box->setVisible(show_line);
        active_line_box->setEnabled(show_line);
    }

    const bool has_templates
        = active_template_combo && active_template_combo->count() > 0;

    if (active_templates_box) {
        const bool show_tpl = has_active && has_templates && !drawing_mode;
        active_templates_box->setVisible(show_tpl);
        active_templates_box->setEnabled(show_tpl);
    }
}

void settings_panel::set_btn_color(QPushButton* btn, const QColor& c) const {
    if (!btn) {
        return;
    }
    btn->setStyleSheet(QString("background-color: %1;").arg(c.name()));
}

void settings_panel::rebuild_log_views() const {
    refresh_log_filter_options();
    rebuild_log_view(add_log_view, frontend_log_area::add);
    rebuild_log_view(event_log_view, frontend_log_area::streams);
    rebuild_log_view(active_log_view, frontend_log_area::active);
}

void settings_panel::refresh_log_filter_options() const {
    QStringList stream_names;
    QStringList subsystem_names;

    if (shared_log_buffer != nullptr) {
        for (const frontend_log_entry& entry : shared_log_buffer->entries()) {
            const QString stream_name = entry.stream_name.trimmed();
            if (!stream_name.isEmpty() && !stream_names.contains(stream_name)) {
                stream_names.push_back(stream_name);
            }

            const QString subsystem_name = entry.subsystem.trimmed();
            if (!subsystem_name.isEmpty()
                && !subsystem_names.contains(subsystem_name)) {
                subsystem_names.push_back(subsystem_name);
            }
        }
    }

    std::sort(
        stream_names.begin(), stream_names.end(),
        [](const QString& lhs, const QString& rhs) {
            return lhs.localeAwareCompare(rhs) < 0;
        }
    );
    std::sort(
        subsystem_names.begin(), subsystem_names.end(),
        [](const QString& lhs, const QString& rhs) {
            return lhs.localeAwareCompare(rhs) < 0;
        }
    );

    settings_panel_support::replace_filter_combo_items(
        log_stream_filter_combo, stream_names, current_log_stream_filter
    );
    settings_panel_support::replace_filter_combo_items(
        log_subsystem_filter_combo, subsystem_names,
        current_log_subsystem_filter
    );
}

bool settings_panel::entry_matches_log_filters(
    const frontend_log_entry& entry
) const {
    const int active_severity_filter = log_severity_filter_combo != nullptr
        ? log_severity_filter_combo->currentData().toInt()
        : current_log_severity_filter;
    const QString active_stream_filter = log_stream_filter_combo != nullptr
        ? log_stream_filter_combo->currentData().toString()
        : current_log_stream_filter;
    const QString active_subsystem_filter
        = log_subsystem_filter_combo != nullptr
        ? log_subsystem_filter_combo->currentData().toString()
        : current_log_subsystem_filter;

    if (current_log_mode == frontend_log_mode::release
        && entry.severity == frontend_log_severity::debug) {
        return false;
    }

    if (active_severity_filter >= 0
        && static_cast<int>(entry.severity) != active_severity_filter) {
        return false;
    }

    if (!active_stream_filter.isEmpty()
        && entry.stream_name != active_stream_filter) {
        return false;
    }

    if (!active_subsystem_filter.isEmpty()
        && entry.subsystem != active_subsystem_filter) {
        return false;
    }

    return true;
}

void settings_panel::rebuild_log_view(
    QPlainTextEdit* view, const frontend_log_area area
) const {
    if (view == nullptr) {
        return;
    }

    view->clear();

    if (shared_log_buffer == nullptr) {
        return;
    }

    QStringList lines;

    for (const frontend_log_entry& entry :
         shared_log_buffer->entries_for_area(area)) {
        if (entry_matches_log_filters(entry)) {
            lines.push_back(format_frontend_log_entry(current_log_mode, entry));
        }
    }

    view->setPlainText(lines.join('\n'));
}

void settings_panel::on_log_mode_changed(const int index) {
    set_log_mode(
        index == 0 ? frontend_log_mode::release : frontend_log_mode::debug
    );
}

void settings_panel::on_log_severity_filter_changed(const int index) {
    Q_UNUSED(index);

    if (log_severity_filter_combo == nullptr) {
        return;
    }

    current_log_severity_filter
        = log_severity_filter_combo->currentData().toInt();
    rebuild_log_views();
}

void settings_panel::on_log_stream_filter_changed(const int index) {
    Q_UNUSED(index);

    if (log_stream_filter_combo == nullptr) {
        return;
    }

    current_log_stream_filter
        = log_stream_filter_combo->currentData().toString();
    rebuild_log_views();
}

void settings_panel::on_log_subsystem_filter_changed(const int index) {
    Q_UNUSED(index);

    if (log_subsystem_filter_combo == nullptr) {
        return;
    }

    current_log_subsystem_filter
        = log_subsystem_filter_combo->currentData().toString();
    rebuild_log_views();
}

void settings_panel::on_active_combo_changed(const QString& text) {
    if (text == str_label("none")) {
        emit active_stream_selected(QString());
    } else {
        emit active_stream_selected(text);
    }
    update_active_tools();
}

void settings_panel::on_active_mode_clicked(int id) {
    emit active_edit_mode_changed(id == 0);
    update_active_tools();
}

void settings_panel::on_active_line_color_clicked() {
    const auto c = QColorDialog::getColor(
        active_line_color, this, str_label("choose color")
    );
    if (!c.isValid()) {
        return;
    }

    active_line_color = c;
    set_btn_color(active_line_color_btn, active_line_color);

    emit active_line_params_changed(
        active_line_name_edit->text().trimmed(), active_line_color,
        active_line_closed_cb->isChecked()
    );
}

void settings_panel::on_active_line_undo_clicked() {
    emit active_line_undo_requested();
}

void settings_panel::on_active_line_save_clicked() {
    emit active_line_save_requested(
        active_line_name_edit->text().trimmed(),
        active_line_closed_cb->isChecked()
    );
}

void settings_panel::on_active_line_name_finished() {
    emit active_line_params_changed(
        active_line_name_edit->text().trimmed(), active_line_color,
        active_line_closed_cb->isChecked()
    );
}

void settings_panel::on_active_line_closed_toggled(bool checked) {
    Q_UNUSED(checked);

    emit active_line_params_changed(
        active_line_name_edit->text().trimmed(), active_line_color,
        active_line_closed_cb->isChecked()
    );
}

void settings_panel::on_active_template_combo_changed(const QString& text) {
    if (text.isEmpty() || text == str_label("none")) {
        emit active_template_selected(QString());
    } else {
        emit active_template_selected(text);
    }
}

void settings_panel::on_active_template_color_clicked() {
    const auto c = QColorDialog::getColor(
        active_template_color, this, str_label("choose color")
    );
    if (!c.isValid()) {
        return;
    }

    active_template_color = c;
    set_btn_color(active_template_color_btn, active_template_color);
    emit active_template_color_changed(active_template_color);
}

void settings_panel::on_active_template_add_clicked() {
    if (!active_template_combo) {
        return;
    }

    const auto t = active_template_combo->currentText();
    if (t.isEmpty() || t == str_label("none")) {
        return;
    }

    emit active_template_add_requested(t, active_template_color);
}

void settings_panel::on_stream_item_changed(QTreeWidgetItem* item, int column) {
    if (!item) {
        return;
    }
    if (column != 0) {
        return;
    }

    const auto name = item->text(1);
    const bool show = item->checkState(0) == Qt::Checked;

    emit show_stream_changed(name, show);

    append_log(
        settings_panel_support::make_log_entry(
            frontend_log_area::streams, frontend_log_severity::info,
            QStringLiteral("settings_panel"),
            show ? QStringLiteral("stream shown in grid")
                 : QStringLiteral("stream hidden from grid"),
            name
        )
    );
}
