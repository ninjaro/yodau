#include "shell/stream_controller.hpp"
#include "geometry/geometry.hpp"
#include "monitor/runtime_bridge.hpp"
#include "shell/str_label.hpp"
#include "streams/stream.hpp"
#include "widgets/settings_panel.hpp"

#include <QCameraDevice>
#include <QColor>
#include <QDateTime>
#include <QImage>
#include <QMediaDevices>
#include <QMetaObject>
#include <QMetaType>
#include <QThread>
#include <QtGlobal>
#include <algorithm>
#include <chrono>
#include <functional>
#include <utility>

#include "streams/event.hpp"
#include "streams/frame.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/stream_board.hpp"
#include "widgets/stream_cell.hpp"

Q_DECLARE_METATYPE(yodau::backend::event)

namespace stream_controller_support {

using steady_clock = std::chrono::steady_clock;

constexpr auto fps_policy_refresh_interval = std::chrono::milliseconds(250);

} // namespace stream_controller_support

stream_controller::stream_controller(
    yodau::backend::stream_manager* mgr, settings_panel* panel,
    stream_board* zone, yodau::monitor::runtime_bridge* monitor, QObject* parent
)
    : QObject(parent)
    , backend_runtime(
          yodau::backend::processing_runtime_options {
              .mode = yodau::backend::render_mode::frontend_only,
              .enable_virtual_camera = false,
          }
      )
    , stream_mgr(mgr)
    , monitor_bridge(monitor)
    , settings(panel)
    , main_zone(zone)
    , grid(zone ? zone->grid_mode() : nullptr)
    , widget_bridge(zone, panel) {
    qRegisterMetaType<yodau::backend::event>("yodau::backend::event");
    qRegisterMetaType<frontend_log_entry>("frontend_log_entry");
    qRegisterMetaType<stream_settings>("stream_settings");
    qRegisterMetaType<line_profile>("line_profile");
    qRegisterMetaType<template_apply_settings>("template_apply_settings");
    fps_capability = yodau::backend::detect_fps_capability_profile();
    log_buffer = new frontend_log_buffer(this);

    if (settings != nullptr) {
        settings->set_log_buffer(log_buffer);
        widget_bridge.initialize_editor_state(edit_session);
    }

    init_from_backend();

    if (stream_mgr) {
        backend_runtime.attach(*stream_mgr);
        stream_mgr->set_event_batch_sink(
            std::bind_front(&stream_controller::on_backend_events, this)
        );
    }

    if (settings && grid) {
        widget_bridge.sync_active_candidates();
        widget_bridge.sync_active_selection(
            route_state.active_stream_name(), stream_settings {}
        );
    }

    setup_settings_connections();
    setup_grid_connections();
    refresh_fps_policy(true);
    update_monitor_inventory();
}

void stream_controller::update_monitor_inventory() {
    if (!stream_mgr || monitor_bridge == nullptr) {
        return;
    }

    const int configured_streams
        = static_cast<int>(stream_mgr->stream_names().size());
    const int visible_streams
        = grid != nullptr ? static_cast<int>(grid->stream_names().size()) : 0;
    const int active_streams = route_state.has_active_stream() ? 1 : 0;
    const int configured_lines
        = static_cast<int>(stream_mgr->line_names().size());

    int detected_local_sources = 0;
    for (const std::string& name : stream_mgr->stream_names()) {
        if (QString::fromStdString(name).startsWith(QStringLiteral("video"))) {
            ++detected_local_sources;
        }
    }

    monitor_bridge->set_inventory(
        configured_streams, visible_streams, active_streams, configured_lines,
        detected_local_sources
    );
}

void stream_controller::init_from_backend() {
    if (!stream_mgr || !settings) {
        return;
    }

    QSet<QString> names;

    const auto backend_names = stream_mgr->stream_names();
    for (auto& n : backend_names) {
        const auto qname = QString::fromStdString(n);
        names.insert(qname);
        catalog_state.ensure_stream(qname);

        QString desc = str_label("<unknown>");
        const auto s = stream_mgr->find_stream(n);
        if (s) {
            // const auto t = s->get_type();
            const auto path = QString::fromStdString(s->get_path());
            const auto type = QString::fromStdString(
                yodau::backend::stream::type_name(s->get_type())
            );
            desc = QString("%1:%2").arg(type, path);
        }

        settings->add_stream_entry(qname, desc);
    }

    settings->set_existing_names(names);
}

void stream_controller::handle_add_file(
    const QString& path, const QString& name, const bool loop
) {
    handle_add_stream_common(path, name, "file", loop);
}

void stream_controller::handle_add_local(
    const QString& source, const QString& name
) {
    handle_add_stream_common(source, name, "local", true);
}

void stream_controller::handle_add_url(
    const QString& url, const QString& name
) {
    handle_add_stream_common(url, name, "url", true);
}

void stream_controller::handle_detect_local_sources() {
    if (!stream_mgr) {
        return;
    }

    stream_mgr->refresh_local_streams();

    const auto backend_names = stream_mgr->stream_names();
    const QStringList locals
        = stream_catalog_state::detected_local_sources(backend_names);

    const auto cams = QMediaDevices::videoInputs();
    if (settings != nullptr) {
        settings->set_local_sources(locals);
    }

    append_log(
        frontend_log_area::add, frontend_log_severity::info,
        QStringLiteral("local_sources"),
        QStringLiteral("local source inventory refreshed"), QString(),
        QStringLiteral("backend=%1 qt=%2").arg(locals.size()).arg(cams.size())
    );
    update_monitor_inventory();
    if (monitor_bridge != nullptr) {
        monitor_bridge->add_marker(QStringLiteral("local_sources_refreshed"));
    }
}

void stream_controller::handle_show_stream_changed(
    const QString& name, const bool show
) {
    if (!grid) {
        return;
    }

    if (show) {
        grid->add_stream(name);
        if (auto* tile = grid->peek_stream_cell(name)) {
            connect(
                tile, &stream_cell::frame_ready, this,
                &stream_controller::on_gui_frame, Qt::UniqueConnection
            );
            tile->set_persistent_lines(edit_session.stream_lines(name));
            tile->set_stream_settings(settings_for_stream(name));

            const auto s = stream_mgr->find_stream(name.toStdString());
            if (s) {
                tile->set_loop(s->is_looping());
                const auto path = QString::fromStdString(s->get_path());
                const auto type = s->get_type();

                if (type == yodau::backend::stream_type::local) {
                    tile->set_camera_id(path.toUtf8());
                } else if (type == yodau::backend::stream_type::file) {
                    tile->set_source(QUrl::fromLocalFile(path));
                } else {
                    tile->set_source(QUrl(path));
                }
            }
        }
    } else {
        grid->remove_stream(name);
        if (route_state.hide_stream(name) && main_zone) {
            if (auto* cell = main_zone->take_active_cell()) {
                cell->deleteLater();
            }
            widget_bridge.sync_active_selection(QString(), stream_settings {});
        }
    }

    widget_bridge.sync_active_candidates();

    append_log(
        frontend_log_area::streams, frontend_log_severity::info,
        QStringLiteral("grid_visibility"),
        show ? QStringLiteral("stream shown in grid")
             : QStringLiteral("stream hidden from grid"),
        name
    );

    emit monitor_stream_visibility_changed(name, show);
    refresh_fps_policy(true);
    update_monitor_inventory();
    if (monitor_bridge != nullptr) {
        monitor_bridge->add_marker(
            show ? QStringLiteral("stream_visible")
                 : QStringLiteral("stream_hidden")
        );
    }
}

void stream_controller::handle_backend_event(const QString& text) {
    append_log(
        frontend_log_area::active, frontend_log_severity::info,
        QStringLiteral("backend_event"), text
    );
}

stream_settings stream_controller::settings_for_stream(const QString& name) const {
    return catalog_state.settings_for(name);
}

QString stream_controller::algorithm_id_for_stream(const QString& name) const {
    return catalog_state.algorithm_id_for(name);
}

void stream_controller::set_active_stream(const QString& name) {
    if (!main_zone) {
        return;
    }

    route_state.set_active_stream(name);
    const QString active_name = route_state.active_stream_name();

    const stream_settings current_stream_settings
        = settings_for_stream(active_name);
    widget_bridge.apply_active_stream(
        active_name, current_stream_settings, edit_session
    );
    widget_bridge.sync_active_persistent(active_name, edit_session);
    refresh_fps_policy(true);
    update_monitor_inventory();

    append_log(
        frontend_log_area::active, frontend_log_severity::info,
        QStringLiteral("active_stream"),
        active_name.isEmpty() ? QStringLiteral("active stream cleared")
                              : QStringLiteral("active stream selected"),
        active_name
    );

    if (monitor_bridge != nullptr) {
        monitor_bridge->add_marker(
            active_name.isEmpty() ? QStringLiteral("active_stream_cleared")
                                  : QStringLiteral("active_stream_selected")
        );
    }
}

void stream_controller::on_active_stream_settings_changed(
    stream_settings settings_value
) {
    settings_value.stream_name = settings_value.stream_name.trimmed();
    settings_value.algorithm_id
        = normalized_frontend_algorithm_id(settings_value.algorithm_id);
    settings_value.algorithm_preset = normalized_algorithm_preset_id(
        settings_value.algorithm_id, settings_value.algorithm_preset
    );

    const QString active_name = route_state.active_stream_name();
    const QString previous_active_name = active_name;
    if (settings_value.stream_name != previous_active_name) {
        set_active_stream(settings_value.stream_name);
        return;
    }

    if (settings_value.stream_name.isEmpty()) {
        return;
    }

    const stream_settings previous_settings
        = settings_for_stream(settings_value.stream_name);
    catalog_state.set_stream_settings(settings_value);
    widget_bridge.sync_stream_visual_settings(
        settings_value.stream_name, settings_value, route_state
    );

    if (settings != nullptr && settings_value.stream_name == active_name) {
        widget_bridge.sync_active_selection(active_name, settings_value);
    }

    if (previous_settings.labels_enabled != settings_value.labels_enabled) {
        append_log(
            frontend_log_area::active, frontend_log_severity::info,
            QStringLiteral("stream_settings"),
            settings_value.labels_enabled
                ? QStringLiteral("line labels enabled")
                : QStringLiteral("line labels disabled"),
            settings_value.stream_name
        );
    }

    if (previous_settings.algorithm_id != settings_value.algorithm_id) {
        const bool baseline_selected
            = settings_value.algorithm_id
            == QStringLiteral("motion_baseline");

        append_log(
            frontend_log_area::active,
            baseline_selected ? frontend_log_severity::info
                              : frontend_log_severity::warning,
            QStringLiteral("stream_settings"),
            QStringLiteral("algorithm preference updated"),
            settings_value.stream_name,
            QStringLiteral("%1; preset=%2 overlay=%3")
                .arg(
                    baseline_selected
                        ? QStringLiteral(
                              "frontend and backend both use baseline processing"
                          )
                        : QStringLiteral(
                              "frontend preference stored; backend runtime still uses baseline processing"
                          )
                )
                .arg(settings_value.algorithm_preset)
                .arg(
                    settings_value.algorithm_overlay_enabled
                        ? QStringLiteral("true")
                        : QStringLiteral("false")
                ),
            settings_value.algorithm_id
        );
    }
}

void stream_controller::on_active_edit_mode_changed(bool drawing_new) {
    const QString active_name = route_state.active_stream_name();
    edit_session.set_drawing_new_mode(drawing_new);

    if (auto* cell = widget_bridge.active_cell()) {
        cell->clear_draft();
        cell->set_drawing_enabled(drawing_new);

        if (drawing_new) {
            const line_profile& draft_line_profile
                = edit_session.draft_line_profile();
            cell->set_draft_params(
                draft_line_profile.name, draft_line_profile.color,
                draft_line_profile.closed, draft_line_profile.width_text,
                draft_line_profile.length_text,
                draft_line_profile.response_text
            );
        } else if (settings) {
            settings->set_active_template_settings(
                edit_session.active_template_settings()
            );
            widget_bridge.apply_template_preview(
                edit_session.active_template_settings().template_name,
                edit_session
            );
        }
    }

    append_log(
        frontend_log_area::active, frontend_log_severity::info,
        QStringLiteral("editing"),
        drawing_new ? QStringLiteral("edit mode set to draw new")
                    : QStringLiteral("edit mode set to use template"),
        active_name
    );
}

void stream_controller::on_active_line_profile_changed(line_profile profile_value) {
    const QString active_name = route_state.active_stream_name();
    edit_session.set_draft_line_profile(std::move(profile_value));
    const line_profile& draft_line_profile = edit_session.draft_line_profile();

    if (settings != nullptr) {
        settings->set_active_line_profile(draft_line_profile);
    }

    if (auto* cell = widget_bridge.active_cell()) {
        cell->set_draft_params(
            draft_line_profile.name, draft_line_profile.color,
            draft_line_profile.closed, draft_line_profile.width_text,
            draft_line_profile.length_text, draft_line_profile.response_text
        );
    }

    append_log(
        frontend_log_area::active, frontend_log_severity::debug,
        QStringLiteral("line_editor"),
        QStringLiteral("active line draft updated"), active_name,
        QStringLiteral("name=%1 color=%2 closed=%3 %4")
            .arg(draft_line_profile.name)
            .arg(draft_line_profile.color.name())
            .arg(
                draft_line_profile.closed ? QStringLiteral("true")
                                          : QStringLiteral("false")
            )
            .arg(
                line_profile_summary_text(
                    draft_line_profile.width_text,
                    draft_line_profile.length_text,
                    draft_line_profile.response_text
                )
            ),
        algorithm_id_for_stream(active_name)
    );
}

void stream_controller::on_active_line_save_requested(line_profile profile_value) {
    const QString active_name = route_state.active_stream_name();
    edit_session.set_draft_line_profile(std::move(profile_value));
    const line_profile& draft_line_profile = edit_session.draft_line_profile();

    append_log(
        frontend_log_area::active, frontend_log_severity::debug,
        QStringLiteral("line_editor"), QStringLiteral("line save requested"),
        active_name,
        QStringLiteral("name=%1 closed=%2 %3")
            .arg(draft_line_profile.name)
            .arg(
                draft_line_profile.closed ? QStringLiteral("true")
                                          : QStringLiteral("false")
            )
            .arg(
                line_profile_summary_text(
                    draft_line_profile.width_text,
                    draft_line_profile.length_text,
                    draft_line_profile.response_text
                )
            ),
        algorithm_id_for_stream(active_name)
    );

    auto* cell = active_cell_checked("add line");
    if (!cell) {
        return;
    }

    const auto pts = cell->draft_points_pct();
    if (pts.size() < 2) {
        append_log(
            frontend_log_area::active, frontend_log_severity::warning,
            QStringLiteral("line_editor"),
            QStringLiteral("line add requires at least 2 points"), active_name,
            QString(), algorithm_id_for_stream(active_name)
        );
        return;
    }

    const auto points_str = points_str_from_pct(pts);
    append_log(
        frontend_log_area::active, frontend_log_severity::debug,
        QStringLiteral("line_editor"),
        QStringLiteral("line draft points prepared"), active_name, points_str,
        algorithm_id_for_stream(active_name)
    );

    try {
        const auto lp = stream_mgr->add_line(
            points_str.toStdString(), draft_line_profile.closed,
            draft_line_profile.name.toStdString()
        );

        const auto final_name = QString::fromStdString(lp->name);
        const stream_cell::line_instance inst = edit_session.store_saved_line(
            active_name, final_name, pts, draft_line_profile.closed
        );

        cell->add_persistent_line(inst);
        stream_mgr->set_line(
            active_name.toStdString(), final_name.toStdString()
        );

        cell->clear_draft();
        cell->set_draft_params(
            QString(), QColor(Qt::red), false, default_line_width_text(),
            default_line_length_text(), default_line_response_text()
        );

        edit_session.reset_draft_line_profile();

        if (settings) {
            settings->set_active_line_profile(edit_session.draft_line_profile());
            settings->reset_active_line_form();
            settings->add_template_candidate(final_name);
            settings->reset_active_template_form();
        }

        append_log(
            frontend_log_area::active, frontend_log_severity::info,
            QStringLiteral("line_editor"), QStringLiteral("line added"),
            active_name,
            QStringLiteral("template=%1 points=%2 closed=%3 %4")
                .arg(final_name)
                .arg(pts.size())
                .arg(
                    inst.closed ? QStringLiteral("true")
                                : QStringLiteral("false")
                )
                .arg(
                    line_profile_summary_text(
                        inst.width_text, inst.length_text, inst.response_text
                    )
                ),
            algorithm_id_for_stream(active_name)
        );

        widget_bridge.sync_active_persistent(active_name, edit_session);
        refresh_fps_policy(true);
        update_monitor_inventory();
        if (monitor_bridge != nullptr) {
            monitor_bridge->add_marker(QStringLiteral("line_added"));
        }
    } catch (const std::exception& e) {
        append_log(
            frontend_log_area::active, frontend_log_severity::error,
            QStringLiteral("line_editor"), QStringLiteral("line add failed"),
            active_name, QString::fromLocal8Bit(e.what()),
            algorithm_id_for_stream(active_name)
        );
    }
}

void stream_controller::on_active_template_add_requested(
    template_apply_settings settings_value
) {
    const QString active_name = route_state.active_stream_name();
    edit_session.set_active_template_settings(std::move(settings_value));
    const template_apply_settings& active_template_settings
        = edit_session.active_template_settings();

    auto* cell = active_cell_checked("add template");
    if (!cell) {
        return;
    }

    if (!edit_session.has_template(active_template_settings.template_name)) {
        append_log(
            frontend_log_area::active, frontend_log_severity::warning,
            QStringLiteral("template_editor"),
            QStringLiteral("template add failed: unknown template"),
            active_name, active_template_settings.template_name,
            algorithm_id_for_stream(active_name)
        );
        return;
    }

    try {
        stream_mgr->set_line(
            active_name.toStdString(),
            active_template_settings.template_name.toStdString()
        );
    } catch (const std::exception& e) {
        append_log(
            frontend_log_area::active, frontend_log_severity::error,
            QStringLiteral("template_editor"),
            QStringLiteral("template add failed"), active_name,
            QString::fromLocal8Bit(e.what()),
            algorithm_id_for_stream(active_name)
        );
        return;
    }

    const stream_cell::line_instance inst = edit_session.store_applied_template_line(
        active_name, active_template_settings
    );
    cell->add_persistent_line(inst);

    append_log(
        frontend_log_area::active, frontend_log_severity::info,
        QStringLiteral("template_editor"),
        QStringLiteral("template added to active stream"), active_name,
        QStringLiteral("%1 %2")
            .arg(active_template_settings.template_name)
            .arg(
                line_profile_summary_text(
                    active_template_settings.width_text,
                    active_template_settings.length_text,
                    active_template_settings.response_text
                )
            ),
        algorithm_id_for_stream(active_name)
    );

    cell->clear_draft();

    if (settings) {
        settings->reset_active_template_form();
    }

    widget_bridge.sync_active_persistent(active_name, edit_session);
}

void stream_controller::on_active_template_settings_changed(
    template_apply_settings settings_value
) {
    const QString active_name = route_state.active_stream_name();
    const QString previous_template_name
        = edit_session.active_template_settings().template_name;
    const bool inherit_template_profile
        = settings_value.template_name.trimmed() != previous_template_name;
    settings_value = edit_session.resolved_template_settings(
        std::move(settings_value), inherit_template_profile
    );
    edit_session.set_active_template_settings(settings_value);
    const template_apply_settings& active_template_settings
        = edit_session.active_template_settings();

    if (settings != nullptr) {
        settings->set_active_template_settings(active_template_settings);
    }

    if (edit_session.drawing_new_mode()) {
        return;
    }

    widget_bridge.apply_template_preview(
        active_template_settings.template_name, edit_session
    );

    append_log(
        frontend_log_area::active, frontend_log_severity::debug,
        QStringLiteral("template_editor"),
        QStringLiteral("template preview updated"), active_name,
        QStringLiteral("template=%1 color=%2 %3")
            .arg(active_template_settings.template_name)
            .arg(active_template_settings.color.name())
            .arg(
                line_profile_summary_text(
                active_template_settings.width_text,
                active_template_settings.length_text,
                active_template_settings.response_text
                )
            ),
        algorithm_id_for_stream(active_name)
    );
}

void stream_controller::on_active_line_undo_requested() {
    auto* cell = widget_bridge.active_cell();
    if (!cell) {
        return;
    }

    auto pts = cell->draft_points_pct();
    if (pts.empty()) {
        return;
    }

    pts.pop_back();
    cell->set_draft_points_pct(pts);
}

void stream_controller::setup_settings_connections() {
    if (!settings || !main_zone) {
        return;
    }

    connect(
        settings, &settings_panel::active_stream_settings_changed, this,
        &stream_controller::on_active_stream_settings_changed
    );

    connect(
        settings, &settings_panel::active_edit_mode_changed, this,
        &stream_controller::on_active_edit_mode_changed
    );

    connect(
        settings, &settings_panel::active_line_profile_changed, this,
        &stream_controller::on_active_line_profile_changed
    );

    connect(
        settings, &settings_panel::active_line_save_requested, this,
        &stream_controller::on_active_line_save_requested
    );

    connect(
        settings, &settings_panel::active_template_add_requested, this,
        &stream_controller::on_active_template_add_requested
    );

    connect(
        settings, &settings_panel::active_line_undo_requested, this,
        &stream_controller::on_active_line_undo_requested
    );

    connect(
        settings, &settings_panel::active_template_settings_changed, this,
        &stream_controller::on_active_template_settings_changed
    );
}

void stream_controller::setup_grid_connections() {
    if (!grid) {
        return;
    }

    connect(
        grid, &grid_view::stream_closed, this,
        &stream_controller::on_grid_stream_closed
    );

    connect(
        grid, &grid_view::stream_enlarge, this,
        &stream_controller::handle_enlarge_requested
    );
}

void stream_controller::on_grid_stream_closed(const QString& name) {
    if (settings) {
        settings->set_stream_checked(name, false);
    }
}

void stream_controller::handle_add_stream_common(
    const QString& source, const QString& name, const QString& type, bool loop
) {
    if (!stream_mgr) {
        return;
    }

    const stream_route_state::add_source_validation validation
        = stream_route_state::validate_add_source(source, type);
    if (!validation.valid) {
        append_log(
            frontend_log_area::add, frontend_log_severity::warning,
            QStringLiteral("stream_add"), validation.message, name,
            validation.detail
        );
        return;
    }

    try {
        const auto& s = stream_mgr->add_stream(
            source.toStdString(), name.toStdString(), type.toStdString(), loop
        );

        const auto final_name = QString::fromStdString(s.get_name());
        const auto source_desc
            = stream_route_state::source_description(source, type);

        append_log(
            frontend_log_area::add, frontend_log_severity::info,
            QStringLiteral("stream_add"), QStringLiteral("stream added"),
            final_name, source_desc
        );

        register_stream_in_ui(final_name, source_desc);
        update_monitor_inventory();
        if (monitor_bridge != nullptr) {
            monitor_bridge->add_marker(QStringLiteral("stream_added"));
        }
    } catch (const std::exception& e) {
        append_log(
            frontend_log_area::add, frontend_log_severity::error,
            QStringLiteral("stream_add"),
            QStringLiteral("add %1 stream failed").arg(type), name,
            QString::fromLocal8Bit(e.what())
        );
    }
}

void stream_controller::register_stream_in_ui(
    const QString& final_name, const QString& source_desc
) {
    catalog_state.ensure_stream(final_name);

    if (!settings) {
        return;
    }

    settings->add_existing_name(final_name);
    settings->add_stream_entry(final_name, source_desc);
    settings->clear_add_inputs();
    widget_bridge.sync_active_candidates();

    refresh_fps_policy(true);
    update_monitor_inventory();
}

void stream_controller::handle_enlarge_requested(const QString& name) {
    const QString next_active_name
        = route_state.next_active_stream_for_enlarge(name);
    if (next_active_name == route_state.active_stream_name()) {
        return;
    }

    set_active_stream(next_active_name);
}

void stream_controller::handle_back_to_grid() {
    set_active_stream(QString());
}

void stream_controller::handle_thumb_activate(const QString& name) {
    handle_enlarge_requested(name);
}

stream_cell*
stream_controller::active_cell_checked(const QString& fail_prefix) {
    if (!stream_mgr || !main_zone || !route_state.has_active_stream()) {
        append_log(
            frontend_log_area::active, frontend_log_severity::warning,
            QStringLiteral("active_stream"),
            QStringLiteral("%1 failed: no active stream").arg(fail_prefix)
        );
        return nullptr;
    }

    auto* cell = widget_bridge.active_cell();
    if (!cell) {
        append_log(
            frontend_log_area::active, frontend_log_severity::warning,
            QStringLiteral("active_stream"),
            QStringLiteral("%1 failed: active cell not found").arg(fail_prefix),
            route_state.active_stream_name()
        );
        return nullptr;
    }

    return cell;
}

void stream_controller::append_log(
    const frontend_log_area area, const frontend_log_severity severity,
    const QString& subsystem, const QString& message,
    const QString& stream_name, const QString& detail,
    const QString& algorithm_id
) const {
    const QString resolved_algorithm_id = stream_name.isEmpty()
        ? QString()
        : (algorithm_id.isEmpty() ? algorithm_id_for_stream(stream_name)
                                  : normalized_frontend_algorithm_id(algorithm_id));

    frontend_log_entry entry {
        .timestamp = QDateTime(),
        .area = area,
        .severity = severity,
        .subsystem = subsystem,
        .stream_name = stream_name,
        .algorithm_id = resolved_algorithm_id,
        .message = message,
        .detail = detail,
    };

    if (log_buffer != nullptr) {
        log_buffer->append(std::move(entry));
        return;
    }

    if (settings != nullptr) {
        settings->append_log(std::move(entry));
    }
}

void stream_controller::log_active(
    const QString& msg, const frontend_log_severity severity,
    const QString& detail
) const {
    append_log(
        frontend_log_area::active, severity, QStringLiteral("active"), msg,
        route_state.active_stream_name(), detail
    );
}

QString
stream_controller::points_str_from_pct(const std::vector<QPointF>& pts) {
    QStringList parts;
    parts.reserve(static_cast<int>(pts.size()));
    for (const auto& p : pts) {
        parts << QString("(%1,%2)").arg(p.x(), 0, 'f', 3).arg(p.y(), 0, 'f', 3);
    }
    return parts.join("; ");
}

void stream_controller::refresh_fps_policy(const bool force) {
    if (!stream_mgr || !grid) {
        return;
    }

    const auto now = stream_controller_support::steady_clock::now();

    if (!force && last_fps_policy_refresh.time_since_epoch().count() != 0) {
        const auto elapsed
            = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_fps_policy_refresh
            );
        if (elapsed < stream_controller_support::fps_policy_refresh_interval) {
            return;
        }
    }

    last_fps_policy_refresh = now;

    const yodau::backend::fps_mode mode = backend_runtime.processing_enabled()
        ? yodau::backend::fps_mode::playback_and_processing
        : yodau::backend::fps_mode::playback_only;

    const int configured_stream_count
        = static_cast<int>(stream_mgr->stream_names().size());
    const int configured_line_count
        = static_cast<int>(stream_mgr->line_names().size());
    const int visible_stream_count
        = static_cast<int>(grid->stream_names().size())
        + (route_state.has_active_stream() ? 1 : 0);
    const int active_stream_count = route_state.has_active_stream() ? 1 : 0;
    const int cell_count = grid_cell_count();
    const int motion_count = feedback_state.recent_motion_count();
    const double load_ratio = current_device_load_ratio();

    const yodau::backend::fps_runtime_factors default_factors {
        .mode = mode,
        .role = yodau::backend::fps_stream_role::grid,
        .configured_stream_count = configured_stream_count,
        .visible_stream_count = visible_stream_count,
        .active_stream_count = active_stream_count,
        .configured_line_count = configured_line_count,
        .stream_line_count = 0,
        .grid_cell_count = cell_count,
        .recent_motion_count = motion_count,
        .device_load_ratio = load_ratio,
    };

    if (mode == yodau::backend::fps_mode::playback_and_processing) {
        const auto default_profile = yodau::backend::recommend_fps_profile(
            fps_capability, default_factors
        );
        stream_mgr->set_analysis_interval_ms(
            default_profile.analysis_interval_ms
        );
    }

    for (const auto& stream_name : stream_mgr->stream_names()) {
        stream_mgr->clear_stream_analysis_interval_ms(stream_name);
    }

    processing_scale_percent_by_stream.clear();

    for (const auto& name : grid->stream_names()) {
        const yodau::backend::fps_runtime_factors factors {
            .mode = mode,
            .role = yodau::backend::fps_stream_role::grid,
            .configured_stream_count = configured_stream_count,
            .visible_stream_count = visible_stream_count,
            .active_stream_count = active_stream_count,
            .configured_line_count = configured_line_count,
            .stream_line_count = line_count_for_stream(name),
            .grid_cell_count = cell_count,
            .recent_motion_count = motion_count,
            .device_load_ratio = load_ratio,
        };
        const auto profile
            = yodau::backend::recommend_fps_profile(fps_capability, factors);

        if (auto* tile = grid->peek_stream_cell(name)) {
            tile->set_repaint_interval_ms(profile.repaint_interval_ms);
        }

        processing_scale_percent_by_stream.insert(
            name, profile.processing_scale_percent
        );

        if (mode == yodau::backend::fps_mode::playback_and_processing) {
            stream_mgr->set_stream_analysis_interval_ms(
                name.toStdString(), profile.analysis_interval_ms
            );
        }
    }

    const QString active_name = route_state.active_stream_name();
    if (!active_name.isEmpty()) {
        const yodau::backend::fps_runtime_factors factors {
            .mode = mode,
            .role = yodau::backend::fps_stream_role::active,
            .configured_stream_count = configured_stream_count,
            .visible_stream_count = visible_stream_count,
            .active_stream_count = active_stream_count,
            .configured_line_count = configured_line_count,
            .stream_line_count = line_count_for_stream(active_name),
            .grid_cell_count = cell_count,
            .recent_motion_count = motion_count,
            .device_load_ratio = load_ratio,
        };
        const auto profile
            = yodau::backend::recommend_fps_profile(fps_capability, factors);

        if (main_zone != nullptr) {
            if (auto* cell = widget_bridge.active_cell()) {
                cell->set_repaint_interval_ms(profile.repaint_interval_ms);
            }
        }

        processing_scale_percent_by_stream.insert(
            active_name, profile.processing_scale_percent
        );

        if (mode == yodau::backend::fps_mode::playback_and_processing) {
            stream_mgr->set_stream_analysis_interval_ms(
                active_name.toStdString(), profile.analysis_interval_ms
            );
        }
    }
}

int stream_controller::grid_cell_count() const {
    if (!grid) {
        return 0;
    }

    const int layout_cells = grid->layout_cell_count();
    if (layout_cells > 0) {
        return layout_cells;
    }

    return static_cast<int>(grid->stream_names().size());
}

int stream_controller::line_count_for_stream(const QString& stream_name) const {
    if (!stream_mgr || stream_name.isEmpty()) {
        return 0;
    }

    const auto stream_ptr = stream_mgr->find_stream(stream_name.toStdString());
    if (!stream_ptr) {
        return 0;
    }

    return static_cast<int>(stream_ptr->line_names().size());
}

double stream_controller::current_device_load_ratio() const {
    return std::clamp(processing_cost_ema_ms / 20.0, 0.0, 2.5);
}

void stream_controller::note_processing_cost_sample(const double elapsed_ms) {
    if (elapsed_ms <= 0.0) {
        return;
    }

    constexpr double smoothing = 0.18;

    if (processing_cost_ema_ms <= 0.0) {
        processing_cost_ema_ms = elapsed_ms;
        return;
    }

    processing_cost_ema_ms
        = processing_cost_ema_ms * (1.0 - smoothing) + elapsed_ms * smoothing;
}

QImage stream_controller::scaled_processing_image(
    const QString& stream_name, const QImage& image
) const {
    if (image.isNull()) {
        return image;
    }

    const int scale_percent = std::clamp(
        processing_scale_percent_by_stream.value(stream_name, 100), 1, 100
    );
    if (scale_percent >= 100) {
        return image;
    }

    const QSize scaled_size(
        std::max(1, image.width() * scale_percent / 100),
        std::max(1, image.height() * scale_percent / 100)
    );

    return image.scaled(
        scaled_size, Qt::IgnoreAspectRatio, Qt::FastTransformation
    );
}

void stream_controller::on_backend_events(
    const std::vector<yodau::backend::event>& evs
) {
    if (monitor_bridge != nullptr) {
        monitor_bridge->record_event_batch(evs);
    }
    for (const auto& e : evs) {
        on_backend_event(e);
    }
}

yodau::backend::frame
stream_controller::frame_from_image(const QImage& image) const {
    QImage img = image;

    if (img.format() != QImage::Format_RGB888) {
        img = img.convertToFormat(QImage::Format_RGB888);
    }

    yodau::backend::frame f;
    f.width = img.width();
    f.height = img.height();
    f.stride = static_cast<int>(img.bytesPerLine());
    f.format = yodau::backend::pixel_format::rgb24;
    f.ts = std::chrono::steady_clock::now();

    const auto* ptr = img.constBits();
    const int bytes = static_cast<int>(img.sizeInBytes());
    if (ptr && bytes > 0) {
        f.data.assign(ptr, ptr + bytes);
    }

    return f;
}

void stream_controller::on_gui_frame(
    const QString& stream_name, const QImage& image
) {
    if (!stream_mgr) {
        return;
    }

    emit monitor_gui_frame_observed(
        stream_name, static_cast<qint64>(image.sizeInBytes())
    );

    if (!backend_runtime.processing_enabled()) {
        return;
    }

    const auto started = stream_controller_support::steady_clock::now();
    auto f = frame_from_image(scaled_processing_image(stream_name, image));
    stream_mgr->push_frame(stream_name.toStdString(), std::move(f));
    const auto finished = stream_controller_support::steady_clock::now();
    const auto elapsed_ms
        = std::chrono::duration<double, std::milli>(finished - started).count();
    note_processing_cost_sample(elapsed_ms);
    refresh_fps_policy(false);
}

void stream_controller::on_backend_event(const yodau::backend::event& e) {
    if (QThread::currentThread() != thread()) {
        const auto event_value = e;
        QMetaObject::invokeMethod(
            this, "on_backend_event_queued", Qt::QueuedConnection,
            Q_ARG(yodau::backend::event, event_value)
        );
        return;
    }

    on_backend_event_queued(e);
}

void stream_controller::on_backend_event_queued(
    yodau::backend::event event_value
) {
    const processing_feedback_state::processed_event feedback
        = feedback_state.consume_event(event_value);

    append_log(
        frontend_log_area::active, feedback.log_severity,
        QStringLiteral("backend_event"), feedback.log_message,
        feedback.stream_name, feedback.log_detail
    );

    emit monitor_backend_event_observed(
        feedback.kind_text
    );

    if (feedback.motion_activity_changed) {
        refresh_fps_policy(false);
    }

    auto* tile = widget_bridge.tile_for_stream_name(
        feedback.stream_name, route_state
    );
    if (!tile) {
        return;
    }

    if (!feedback.overlay_position_pct.has_value()
        || !feedback.allow_gui_overlay) {
        return;
    }

    if (feedback.tripwire_visual.has_value() && !feedback.line_name.isEmpty()) {
        tile->highlight_line_at(
            feedback.line_name, *feedback.overlay_position_pct,
            feedback.tripwire_visual->strength,
            feedback.tripwire_visual->direction,
            feedback.tripwire_visual->speed
        );
    }

    tile->add_event(*feedback.overlay_position_pct, feedback.overlay_color);
}
