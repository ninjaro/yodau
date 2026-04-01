#include "widgets/settings_panel.hpp"
#include "shell/str_label.hpp"
#include "widgets/active_editor_panel.hpp"
#include "widgets/log_toolbar_panel.hpp"
#include "widgets/stream_inventory_panel.hpp"
#include "widgets/stream_source_panel.hpp"

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QTabWidget>
#include <QVBoxLayout>

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

} // namespace settings_panel_support

settings_panel::settings_panel(QWidget* parent)
    : QWidget(parent)
    , tabs(new QTabWidget(this))
    , add_tab(nullptr)
    , streams_tab(nullptr) {
    qRegisterMetaType<stream_settings>("stream_settings");
    qRegisterMetaType<frontend_log_mode>("frontend_log_mode");
    qRegisterMetaType<line_profile>("line_profile");
    qRegisterMetaType<template_apply_settings>("template_apply_settings");

    build_ui();
    set_active_stream_settings(stream_settings {});
    set_active_line_profile(line_profile {});
    set_active_template_settings(template_apply_settings {});
}

void settings_panel::set_existing_names(QSet<QString> names) {
    existing_names = std::move(names);
    if (source_panel != nullptr) {
        source_panel->set_existing_names(existing_names);
    }
}

void settings_panel::add_existing_name(const QString& name) {
    if (name.isEmpty()) {
        return;
    }
    existing_names.insert(name);
    if (source_panel != nullptr) {
        source_panel->add_existing_name(name);
    }
}

void settings_panel::remove_existing_name(const QString& name) {
    existing_names.remove(name);
    if (source_panel != nullptr) {
        source_panel->remove_existing_name(name);
    }
}

void settings_panel::set_log_buffer(frontend_log_buffer* buffer) {
    if (log_toolbar_widget != nullptr) {
        log_toolbar_widget->set_log_buffer(buffer);
    }
}

void settings_panel::set_log_mode(const frontend_log_mode mode) {
    if (log_toolbar_widget != nullptr) {
        log_toolbar_widget->set_log_mode(mode);
    }
}

frontend_log_mode settings_panel::log_mode() const {
    return log_toolbar_widget != nullptr ? log_toolbar_widget->log_mode()
                                         : frontend_log_mode::release;
}

void settings_panel::append_log(frontend_log_entry entry) const {
    if (log_toolbar_widget != nullptr && log_toolbar_widget->append_entry(entry)) {
        return;
    }

    switch (entry.area) {
    case frontend_log_area::add:
        if (source_panel != nullptr) {
            source_panel->append_log_entry(entry);
        }
        break;
    case frontend_log_area::streams:
        if (inventory_panel != nullptr) {
            inventory_panel->append_log_entry(entry);
        }
        break;
    case frontend_log_area::active:
        if (active_editor_panel_widget != nullptr) {
            active_editor_panel_widget->append_log_entry(entry);
        }
        break;
    }
}

QString settings_panel::compose_current_log_report() const {
    return log_toolbar_widget != nullptr
        ? log_toolbar_widget->compose_log_report(current_log_area())
        : QStringLiteral("yodau log report\n(no matching entries)");
}

QString settings_panel::compose_current_log_summary() const {
    return log_toolbar_widget != nullptr
        ? log_toolbar_widget->compose_log_summary(current_log_area())
        : QStringLiteral("yodau log summary\nentries=0 debug=0 info=0 warn=0 error=0");
}

bool settings_panel::write_current_log_report(const QString& path) const {
    return log_toolbar_widget != nullptr
        && log_toolbar_widget->write_log_report(current_log_area(), path);
}

void settings_panel::add_stream_entry(
    const QString& name, const QString& source, const bool checked
) const {
    if (inventory_panel == nullptr) {
        return;
    }

    inventory_panel->add_stream_entry(name, source, checked);
}

void settings_panel::set_stream_checked(
    const QString& name, const bool checked
) const {
    if (inventory_panel == nullptr) {
        return;
    }

    inventory_panel->set_stream_checked(name, checked);
}

void settings_panel::remove_stream_entry(const QString& name) const {
    if (inventory_panel == nullptr) {
        return;
    }

    inventory_panel->remove_stream_entry(name);
}

void settings_panel::clear_stream_entries() {
    if (inventory_panel != nullptr) {
        inventory_panel->clear_stream_entries();
    }
    existing_names.clear();
    if (source_panel != nullptr) {
        source_panel->set_existing_names(existing_names);
    }
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
    if (source_panel != nullptr) {
        source_panel->set_local_sources(sources);
    }
}

void settings_panel::clear_add_inputs() const {
    if (source_panel != nullptr) {
        source_panel->clear_inputs();
    }
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
    if (active_editor_panel_widget == nullptr) {
        return;
    }

    active_editor_panel_widget->set_active_candidates(names);
}

void settings_panel::set_active_current(const QString& name) const {
    if (active_editor_panel_widget == nullptr) {
        return;
    }

    active_editor_panel_widget->set_active_current(name);
}

void settings_panel::set_active_stream_settings(
    const stream_settings& settings_value
) {
    if (active_editor_panel_widget == nullptr) {
        return;
    }

    active_editor_panel_widget->set_stream_settings(settings_value);
}

stream_settings settings_panel::current_active_stream_settings() const {
    return active_editor_panel_widget != nullptr
        ? active_editor_panel_widget->current_stream_settings()
        : stream_settings {};
}

void settings_panel::add_template_candidate(const QString& name) const {
    if (active_editor_panel_widget == nullptr) {
        return;
    }

    active_editor_panel_widget->add_template_candidate(name);
}

void settings_panel::set_template_candidates(const QStringList& names) const {
    if (active_editor_panel_widget == nullptr) {
        return;
    }

    active_editor_panel_widget->set_template_candidates(names);
}

void settings_panel::set_active_line_profile(const line_profile& profile) {
    if (active_editor_panel_widget == nullptr) {
        return;
    }

    active_editor_panel_widget->set_line_profile(profile);
}

line_profile settings_panel::current_active_line_profile() const {
    return active_editor_panel_widget != nullptr
        ? active_editor_panel_widget->current_line_profile()
        : line_profile {};
}

void settings_panel::set_active_template_settings(
    const template_apply_settings& settings_value
) {
    if (active_editor_panel_widget == nullptr) {
        return;
    }

    active_editor_panel_widget->set_template_settings(settings_value);
}

template_apply_settings settings_panel::current_active_template_settings() const {
    return active_editor_panel_widget != nullptr
        ? active_editor_panel_widget->current_template_settings()
        : template_apply_settings {};
}

void settings_panel::reset_active_line_form() {
    if (active_editor_panel_widget == nullptr) {
        return;
    }

    active_editor_panel_widget->reset_line_form();
}

void settings_panel::reset_active_template_form() {
    if (active_editor_panel_widget == nullptr) {
        return;
    }

    active_editor_panel_widget->reset_template_form();
}

void settings_panel::set_active_line_closed(bool closed) const {
    if (active_editor_panel_widget == nullptr) {
        return;
    }

    active_editor_panel_widget->set_line_closed(closed);
}

QString settings_panel::active_template_current() const {
    if (active_editor_panel_widget == nullptr) {
        return {};
    }

    return active_editor_panel_widget->current_template_name();
}

QColor settings_panel::active_template_preview_color() const {
    return active_editor_panel_widget != nullptr
        ? active_editor_panel_widget->preview_color()
        : QColor(Qt::red);
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
    if (active_editor_panel_widget != nullptr) {
        active_editor_panel_widget->clear_log();
    }
}

void settings_panel::build_ui() {
    const auto root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(8, 8, 8, 8);
    log_toolbar_widget = new log_toolbar_panel(this);
    root_layout->addWidget(tabs, 1);
    root_layout->addWidget(log_toolbar_widget);
    connect(
        log_toolbar_widget, &log_toolbar_panel::copy_logs_requested, this,
        &settings_panel::on_copy_logs_clicked
    );
    connect(
        log_toolbar_widget, &log_toolbar_panel::log_mode_changed, this,
        &settings_panel::log_mode_changed
    );
    connect(
        log_toolbar_widget, &log_toolbar_panel::copy_summary_requested, this,
        &settings_panel::on_copy_summary_clicked
    );
    connect(
        log_toolbar_widget, &log_toolbar_panel::save_logs_requested, this,
        &settings_panel::on_save_logs_clicked
    );

    add_tab = build_add_tab();
    streams_tab = build_streams_tab();
    active_tab = build_active_tab();

    tabs->addTab(add_tab, str_label("add stream"));
    tabs->addTab(streams_tab, str_label("streams"));
    tabs->addTab(active_tab, str_label("active"));
}

QWidget* settings_panel::build_add_tab() {
    source_panel = new stream_source_panel(this);
    source_panel->set_existing_names(existing_names);
    source_panel->set_log_toolbar(log_toolbar_widget);
    connect(
        source_panel, &stream_source_panel::add_file_stream, this,
        &settings_panel::add_file_stream
    );
    connect(
        source_panel, &stream_source_panel::add_local_stream, this,
        &settings_panel::add_local_stream
    );
    connect(
        source_panel, &stream_source_panel::add_url_stream, this,
        &settings_panel::add_url_stream
    );
    connect(
        source_panel, &stream_source_panel::detect_local_sources_requested, this,
        &settings_panel::detect_local_sources_requested
    );
    connect(
        source_panel, &stream_source_panel::log_requested, this,
        [this](frontend_log_entry entry) { append_log(entry); }
    );
    return source_panel;
}

QWidget* settings_panel::build_streams_tab() {
    inventory_panel = new stream_inventory_panel(this);
    inventory_panel->set_log_toolbar(log_toolbar_widget);
    connect(
        inventory_panel, &stream_inventory_panel::show_stream_changed, this,
        [this](const QString& name, const bool show) {
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
    );
    return inventory_panel;
}

QWidget* settings_panel::build_active_tab() {
    active_editor_panel_widget = new active_editor_panel(this);
    active_editor_panel_widget->set_log_toolbar(log_toolbar_widget);
    connect(
        active_editor_panel_widget, &active_editor_panel::stream_settings_changed,
        this, &settings_panel::active_stream_settings_changed
    );
    connect(
        active_editor_panel_widget, &active_editor_panel::edit_mode_changed, this,
        &settings_panel::active_edit_mode_changed
    );
    connect(
        active_editor_panel_widget, &active_editor_panel::line_profile_changed,
        this,
        &settings_panel::active_line_profile_changed
    );
    connect(
        active_editor_panel_widget, &active_editor_panel::line_save_requested,
        this,
        &settings_panel::active_line_save_requested
    );
    connect(
        active_editor_panel_widget, &active_editor_panel::line_undo_requested,
        this,
        &settings_panel::active_line_undo_requested
    );
    connect(
        active_editor_panel_widget,
        &active_editor_panel::template_settings_changed, this,
        &settings_panel::active_template_settings_changed
    );
    connect(
        active_editor_panel_widget, &active_editor_panel::template_add_requested,
        this,
        &settings_panel::active_template_add_requested
    );
    return active_editor_panel_widget;
}

frontend_log_area settings_panel::current_log_area() const {
    if (tabs == nullptr) {
        return frontend_log_area::active;
    }

    switch (tabs->currentIndex()) {
    case 0:
        return frontend_log_area::add;
    case 1:
        return frontend_log_area::streams;
    case 2:
    default:
        return frontend_log_area::active;
    }
}

void settings_panel::on_copy_logs_clicked() {
    if (QApplication::clipboard() == nullptr) {
        return;
    }

    QApplication::clipboard()->setText(compose_current_log_report());
}

void settings_panel::on_copy_summary_clicked() {
    if (QApplication::clipboard() == nullptr) {
        return;
    }

    QApplication::clipboard()->setText(compose_current_log_summary());
}

void settings_panel::on_save_logs_clicked() {
    const QString default_name = QStringLiteral(
        "yodau-log-report.txt"
    );
    const QString path = QFileDialog::getSaveFileName(
        this, str_label("save log report"), default_name,
        str_label("Text files (*.txt);;All files (*)")
    );
    if (path.isEmpty()) {
        return;
    }

    write_current_log_report(path);
}
