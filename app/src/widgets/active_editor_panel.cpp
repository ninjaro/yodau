#include "widgets/active_editor_panel.hpp"

#include "widgets/active_stream_panel.hpp"
#include "widgets/line_profile_panel.hpp"
#include "widgets/log_area_view.hpp"
#include "widgets/template_apply_panel.hpp"

#include <QSizePolicy>
#include <QVBoxLayout>

active_editor_panel::active_editor_panel(QWidget* parent)
    : QWidget(parent) {
    build_ui();
    set_stream_settings(stream_settings {});
    set_line_profile(line_profile {});
    set_template_settings(template_apply_settings {});
}

void active_editor_panel::set_log_toolbar(log_toolbar_panel* toolbar) {
    if (active_log_view != nullptr) {
        active_log_view->set_log_toolbar(toolbar);
    }
}

void active_editor_panel::set_active_candidates(const QStringList& names) const {
    if (active_stream_panel_widget == nullptr) {
        return;
    }

    active_stream_panel_widget->set_active_candidates(names);
    update_tools();
}

void active_editor_panel::set_active_current(const QString& name) const {
    if (active_stream_panel_widget == nullptr) {
        return;
    }

    active_stream_panel_widget->set_active_current(name);
    update_tools();
}

void active_editor_panel::set_stream_settings(
    const stream_settings& settings_value
) {
    if (active_stream_panel_widget == nullptr) {
        return;
    }

    active_stream_panel_widget->set_stream_settings(settings_value);
    update_tools();
}

stream_settings active_editor_panel::current_stream_settings() const {
    return active_stream_panel_widget != nullptr
        ? active_stream_panel_widget->current_stream_settings()
        : stream_settings {};
}

void active_editor_panel::add_template_candidate(const QString& name) const {
    if (active_template_panel == nullptr || name.isEmpty()) {
        return;
    }

    active_template_panel->add_template_candidate(name);
    update_tools();
}

void active_editor_panel::set_template_candidates(const QStringList& names) const {
    if (active_template_panel == nullptr) {
        return;
    }

    active_template_panel->set_template_candidates(names);
    update_tools();
}

void active_editor_panel::set_line_profile(const line_profile& profile) {
    if (active_line_panel == nullptr) {
        return;
    }

    active_line_panel->set_line_profile(profile);
}

line_profile active_editor_panel::current_line_profile() const {
    return active_line_panel != nullptr ? active_line_panel->current_line_profile()
                                        : line_profile {};
}

void active_editor_panel::set_template_settings(
    const template_apply_settings& settings_value
) {
    if (active_template_panel == nullptr) {
        return;
    }

    active_template_panel->set_template_settings(settings_value);
}

template_apply_settings active_editor_panel::current_template_settings() const {
    return active_template_panel != nullptr
        ? active_template_panel->current_template_settings()
        : template_apply_settings {};
}

void active_editor_panel::reset_line_form() {
    if (active_line_panel == nullptr) {
        return;
    }

    active_line_panel->reset_form();
    emit line_profile_changed(current_line_profile());
}

void active_editor_panel::reset_template_form() {
    if (active_template_panel == nullptr) {
        return;
    }

    active_template_panel->reset_form();
    emit template_settings_changed(current_template_settings());
}

void active_editor_panel::set_line_closed(const bool closed) const {
    if (active_line_panel == nullptr) {
        return;
    }

    active_line_panel->set_line_closed(closed);
}

QString active_editor_panel::current_template_name() const {
    if (active_template_panel == nullptr) {
        return {};
    }

    return active_template_panel->current_template_name();
}

QColor active_editor_panel::preview_color() const {
    return active_template_panel != nullptr
        ? active_template_panel->preview_color()
        : QColor(Qt::red);
}

bool active_editor_panel::append_log_entry(
    const frontend_log_entry& entry
) const {
    return active_log_view != nullptr && active_log_view->append_entry(entry);
}

void active_editor_panel::clear_log() const {
    if (active_log_view != nullptr) {
        active_log_view->clear();
    }
}

void active_editor_panel::build_ui() {
    setObjectName(QStringLiteral("settings_active_editor_panel"));

    const auto layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    active_stream_panel_widget = new active_stream_panel(this);
    layout->addWidget(active_stream_panel_widget);
    connect(
        active_stream_panel_widget, &active_stream_panel::stream_settings_changed,
        this, [this](stream_settings settings_value) {
            update_tools();
            emit stream_settings_changed(settings_value);
        }
    );
    connect(
        active_stream_panel_widget, &active_stream_panel::edit_mode_changed, this,
        [this](const bool drawing_new) {
            update_tools();
            emit edit_mode_changed(drawing_new);
        }
    );

    active_line_panel = new line_profile_panel(this);
    active_line_panel->setObjectName(
        QStringLiteral("settings_active_line_profile_panel")
    );
    layout->addWidget(active_line_panel);
    connect(
        active_line_panel, &line_profile_panel::profile_changed, this,
        &active_editor_panel::line_profile_changed
    );
    connect(
        active_line_panel, &line_profile_panel::save_requested, this,
        &active_editor_panel::line_save_requested
    );
    connect(
        active_line_panel, &line_profile_panel::undo_requested, this,
        &active_editor_panel::line_undo_requested
    );

    active_template_panel = new template_apply_panel(this);
    active_template_panel->setObjectName(
        QStringLiteral("settings_active_template_apply_panel")
    );
    layout->addWidget(active_template_panel);
    connect(
        active_template_panel, &template_apply_panel::settings_changed, this,
        &active_editor_panel::template_settings_changed
    );
    connect(
        active_template_panel, &template_apply_panel::add_requested, this,
        &active_editor_panel::template_add_requested
    );

    active_log_view = new log_area_view(frontend_log_area::active, this);
    active_log_view->setObjectName(QStringLiteral("settings_active_log_view"));
    active_log_view->setMinimumHeight(160);
    active_log_view->setSizePolicy(
        QSizePolicy::Expanding, QSizePolicy::Expanding
    );
    layout->addWidget(active_log_view, 1);

    update_tools();
}

void active_editor_panel::update_tools() const {
    if (active_stream_panel_widget == nullptr) {
        return;
    }

    const bool has_active = active_stream_panel_widget->has_active_stream();
    const bool drawing_mode = active_stream_panel_widget->drawing_new_mode();

    if (active_line_panel != nullptr) {
        active_line_panel->set_panel_active(has_active && drawing_mode);
    }

    if (active_template_panel != nullptr) {
        const bool has_templates = active_template_panel->has_template_candidates();
        active_template_panel->set_panel_active(
            has_active && has_templates && !drawing_mode
        );
    }
}
