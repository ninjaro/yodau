#include "widgets/settings_panel.hpp"
#include "shell/str_label.hpp"
#include "widgets/active_editor_panel.hpp"
#include "widgets/active_stream_panel.hpp"
#include "widgets/log_toolbar_panel.hpp"
#include "widgets/stream_inventory_panel.hpp"
#include "widgets/stream_source_panel.hpp"

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace settings_panel_support {

app_log_entry make_log_entry(
    const app_log_area area, const app_log_severity severity,
    const QString& subsystem, const QString& message,
    const QString& stream_name = QString(), const QString& detail = QString(),
    const QString& algorithm_id = QString()
) {
    app_log_entry entry;
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
    , streams_tab(nullptr) {
    qRegisterMetaType<stream_settings>("stream_settings");
    qRegisterMetaType<app_log_mode>("app_log_mode");
    qRegisterMetaType<line_profile>("line_profile");
    qRegisterMetaType<template_apply_settings>("template_apply_settings");
    qRegisterMetaType<line_edit_request>("line_edit_request");

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
    sync_stream_settings_candidates();
}

void settings_panel::add_existing_name(const QString& name) {
    if (name.isEmpty()) {
        return;
    }
    existing_names.insert(name);
    if (source_panel != nullptr) {
        source_panel->add_existing_name(name);
    }
    sync_stream_settings_candidates();
}

void settings_panel::remove_existing_name(const QString& name) {
    existing_names.remove(name);
    if (source_panel != nullptr) {
        source_panel->remove_existing_name(name);
    }
    sync_stream_settings_candidates();
}

void settings_panel::set_log_buffer(app_log_buffer* buffer) {
    if (log_toolbar_widget != nullptr) {
        log_toolbar_widget->set_log_buffer(buffer);
    }
}

void settings_panel::set_log_mode(const app_log_mode mode) {
    if (log_toolbar_widget != nullptr) {
        log_toolbar_widget->set_log_mode(mode);
    }
}

app_log_mode settings_panel::log_mode() const {
    return log_toolbar_widget != nullptr ? log_toolbar_widget->log_mode()
                                         : app_log_mode::release;
}

void settings_panel::append_log(app_log_entry entry) const {
    if (log_toolbar_widget != nullptr) {
        log_toolbar_widget->append_entry(std::move(entry));
    }
}

QString settings_panel::compose_current_log_report() const {
    return log_toolbar_widget != nullptr
        ? log_toolbar_widget->compose_log_report(std::nullopt)
        : QStringLiteral("yodau log report\n(no matching entries)");
}

QString settings_panel::compose_current_log_summary() const {
    return log_toolbar_widget != nullptr
        ? log_toolbar_widget->compose_log_summary(std::nullopt)
        : QStringLiteral("yodau log summary\nentries=0 debug=0 info=0 warn=0 error=0");
}

bool settings_panel::write_current_log_report(const QString& path) const {
    return log_toolbar_widget != nullptr
        && log_toolbar_widget->write_log_report(std::nullopt, path);
}

log_toolbar_panel* settings_panel::take_log_toolbar_widget() {
    if (log_toolbar_widget == nullptr) {
        return nullptr;
    }

    if (auto* root_layout = layout()) {
        root_layout->removeWidget(log_toolbar_widget);
    }

    log_toolbar_widget->setParent(nullptr);
    return log_toolbar_widget;
}

QWidget* settings_panel::take_active_editor_widget() {
    if (active_editor_panel_widget == nullptr) {
        return nullptr;
    }

    if (auto* root_layout = layout()) {
        root_layout->removeWidget(active_editor_panel_widget);
    }

    active_editor_panel_widget->setParent(nullptr);
    return active_editor_panel_widget;
}

void settings_panel::add_stream_entry(
    const QString& name, const QString& source, const bool checked
) const {
    if (inventory_panel == nullptr) {
        return;
    }

    inventory_panel->add_stream_entry(name, source, checked);
    if (!name.isEmpty()) {
        const_cast<settings_panel*>(this)->existing_names.insert(name);
        sync_stream_settings_candidates();
    }
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
    if (!name.isEmpty()) {
        const_cast<settings_panel*>(this)->existing_names.remove(name);
        if (source_panel != nullptr) {
            source_panel->remove_existing_name(name);
        }
        sync_stream_settings_candidates();
    }
}

void settings_panel::clear_stream_entries() {
    if (inventory_panel != nullptr) {
        inventory_panel->clear_stream_entries();
    }
    existing_names.clear();
    if (source_panel != nullptr) {
        source_panel->set_existing_names(existing_names);
    }
    sync_stream_settings_candidates();
}

void settings_panel::append_event(const QString& text) const {
    append_log(
        settings_panel_support::make_log_entry(
            app_log_area::streams, app_log_severity::info,
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
            app_log_area::add, app_log_severity::info,
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
    if (stream_settings_panel_widget == nullptr) {
        return;
    }

    stream_settings_panel_widget->set_stream_settings(settings_value);
}

stream_settings settings_panel::current_active_stream_settings() const {
    return stream_settings_panel_widget != nullptr
        ? stream_settings_panel_widget->current_stream_settings()
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

void settings_panel::set_active_lines(
    const std::vector<stream_cell::line_instance>& lines
) {
    if (active_editor_panel_widget == nullptr) {
        return;
    }

    active_editor_panel_widget->set_active_lines(lines);
}

bool settings_panel::select_active_line_edit_point(const int visible_index) const {
    return active_editor_panel_widget != nullptr
        && active_editor_panel_widget->select_line_edit_point(visible_index);
}

bool settings_panel::translate_active_line_edit_shape(
    const QPointF& delta_pct
) const {
    return active_editor_panel_widget != nullptr
        && active_editor_panel_widget->translate_line_edit_shape(delta_pct);
}

bool settings_panel::move_active_line_edit_point(
    const int visible_index, const QPointF& point_pct
) const {
    return active_editor_panel_widget != nullptr
        && active_editor_panel_widget->move_line_edit_point(
            visible_index, point_pct
        );
}

bool settings_panel::split_active_line_edit_point(const int visible_index) const {
    return active_editor_panel_widget != nullptr
        && active_editor_panel_widget->split_line_edit_point(visible_index);
}

bool settings_panel::insert_active_line_edit_point_after(
    const int visible_segment_index, const QPointF& point_pct
) const {
    return active_editor_panel_widget != nullptr
        && active_editor_panel_widget->insert_line_edit_point_after(
            visible_segment_index, point_pct
        );
}

bool settings_panel::delete_active_line_edit_point(const int visible_index) const {
    return active_editor_panel_widget != nullptr
        && active_editor_panel_widget->delete_line_edit_point(visible_index);
}

bool settings_panel::rotate_active_line_edit_shape(
    const double delta_degrees, const int visible_pivot_index
) const {
    return active_editor_panel_widget != nullptr
        && active_editor_panel_widget->rotate_line_edit_shape(
            delta_degrees, visible_pivot_index
        );
}

void settings_panel::begin_active_line_edit_change() const {
    if (active_editor_panel_widget != nullptr) {
        active_editor_panel_widget->begin_line_edit_change();
    }
}

void settings_panel::finish_active_line_edit_change() const {
    if (active_editor_panel_widget != nullptr) {
        active_editor_panel_widget->finish_line_edit_change();
    }
}

bool settings_panel::undo_active_line_edit_change() const {
    return active_editor_panel_widget != nullptr
        && active_editor_panel_widget->undo_line_edit_change();
}

bool settings_panel::redo_active_line_edit_change() const {
    return active_editor_panel_widget != nullptr
        && active_editor_panel_widget->redo_line_edit_change();
}

bool settings_panel::revert_active_line_edit_changes() const {
    return active_editor_panel_widget != nullptr
        && active_editor_panel_widget->revert_line_edit_changes();
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
            app_log_area::active, app_log_severity::info,
            QStringLiteral("settings_panel"), msg
        )
    );
}

void settings_panel::clear_active_log() const {
    Q_UNUSED(this);
}

void settings_panel::build_ui() {
    const auto root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(8, 8, 8, 8);
    tabs->setObjectName(QStringLiteral("settings_tabs"));
    log_toolbar_widget = new log_toolbar_panel(this);
    root_layout->addWidget(tabs, 1);
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

    streams_tab = build_streams_tab();
    stream_settings_tab = build_stream_settings_tab();
    active_editor_panel_widget = new active_editor_panel(this);

    tabs->addTab(streams_tab, str_label("streams"));
    tabs->addTab(stream_settings_tab, str_label("stream settings"));

    connect(
        active_editor_panel_widget,
        &active_editor_panel::active_stream_selected, this,
        &settings_panel::active_stream_selected
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
        active_editor_panel_widget, &active_editor_panel::line_enabled_changed,
        this,
        &settings_panel::active_line_enabled_changed
    );
    connect(
        active_editor_panel_widget, &active_editor_panel::line_detach_requested,
        this,
        &settings_panel::active_line_detach_requested
    );
    connect(
        active_editor_panel_widget,
        &active_editor_panel::line_edit_preview_changed, this,
        &settings_panel::active_line_edit_preview_changed
    );
    connect(
        active_editor_panel_widget,
        &active_editor_panel::line_edit_preview_cleared, this,
        &settings_panel::active_line_edit_preview_cleared
    );
    connect(
        active_editor_panel_widget, &active_editor_panel::line_edit_save_requested,
        this,
        &settings_panel::active_line_edit_save_requested
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
}

QWidget* settings_panel::build_streams_tab() {
    const auto tab = new QWidget(this);
    tab->setObjectName(QStringLiteral("settings_streams_tab"));

    const auto layout = new QVBoxLayout(tab);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    const auto summary_label = new QLabel(
        QStringLiteral(
            "Add new sources and manage which configured streams stay visible "
            "in the grid from one tab."
        ),
        tab
    );
    summary_label->setObjectName(QStringLiteral("settings_streams_intro_label"));
    summary_label->setWordWrap(true);
    layout->addWidget(summary_label);

    source_panel = new stream_source_panel(tab);
    source_panel->set_existing_names(existing_names);
    layout->addWidget(source_panel);

    inventory_panel = new stream_inventory_panel(tab);
    layout->addWidget(inventory_panel, 1);

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
        [this](app_log_entry entry) { append_log(entry); }
    );
    connect(
        inventory_panel, &stream_inventory_panel::show_stream_changed, this,
        [this](const QString& name, const bool show) {
            emit show_stream_changed(name, show);
            append_log(
                settings_panel_support::make_log_entry(
                    app_log_area::streams, app_log_severity::info,
                    QStringLiteral("settings_panel"),
                    show ? QStringLiteral("stream shown in grid")
                         : QStringLiteral("stream hidden from grid"),
                    name
                )
            );
        }
    );
    return tab;
}

QWidget* settings_panel::build_stream_settings_tab() {
    stream_settings_panel_widget = new active_stream_panel(
        active_stream_panel::panel_mode::stream_settings,
        QStringLiteral("settings_stream_editor"), this
    );
    stream_settings_panel_widget->setObjectName(
        QStringLiteral("settings_stream_settings_tab")
    );
    connect(
        stream_settings_panel_widget, &active_stream_panel::stream_selected, this,
        &settings_panel::stream_settings_selection_changed
    );
    connect(
        stream_settings_panel_widget,
        &active_stream_panel::stream_settings_changed,
        this, &settings_panel::active_stream_settings_changed
    );
    sync_stream_settings_candidates();
    return stream_settings_panel_widget;
}

QStringList settings_panel::configured_stream_names() const {
    QStringList names = existing_names.values();
    std::sort(names.begin(), names.end());
    return names;
}

void settings_panel::sync_stream_settings_candidates() const {
    if (stream_settings_panel_widget == nullptr) {
        return;
    }

    const QStringList names = configured_stream_names();
    stream_settings_panel_widget->set_active_candidates(names);
    if (stream_settings_panel_widget->current_stream_settings().stream_name.isEmpty()
        && !names.isEmpty()) {
        stream_settings_panel_widget->set_active_current(names.front());
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
        str_label(
            "Text files (*.txt);;TSV files (*.tsv);;JSON files (*.json);;All files (*)"
        )
    );
    if (path.isEmpty()) {
        return;
    }

    write_current_log_report(path);
}
