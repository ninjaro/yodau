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
constexpr auto motion_activity_window = std::chrono::seconds(3);

struct tripwire_visual_payload {
    QString direction;
    double strength { 1.0 };
    double speed { 1.0 };
};

tripwire_visual_payload
parse_tripwire_visual_payload(const std::string& message) {
    tripwire_visual_payload payload;
    if (message.empty()) {
        return payload;
    }

    const auto parts = QString::fromStdString(message).split('|');
    if (!parts.isEmpty()) {
        payload.direction = parts[0].trimmed();
    }

    if (parts.size() >= 2) {
        bool ok = false;
        const double value = parts[1].toDouble(&ok);
        if (ok && value > 0.0) {
            payload.strength = value;
        }
    }

    if (parts.size() >= 3) {
        bool ok = false;
        const double value = parts[2].toDouble(&ok);
        if (ok && value > 0.0) {
            payload.speed = value;
        }
    } else {
        payload.speed = payload.strength;
    }

    return payload;
}

void prune_expired_motion_events(
    std::deque<steady_clock::time_point>& motion_events,
    const steady_clock::time_point now
) {
    while (!motion_events.empty()
           && now - motion_events.front() > motion_activity_window) {
        motion_events.pop_front();
    }
}

} // namespace stream_controller_support

QString
stream_controller::backend_event_kind_text(yodau::backend::event_kind kind) {
    switch (kind) {
    case yodau::backend::event_kind::motion:
        return QStringLiteral("motion");
    case yodau::backend::event_kind::tripwire:
        return QStringLiteral("tripwire");
    case yodau::backend::event_kind::roi:
        return QStringLiteral("roi");
    case yodau::backend::event_kind::info:
    default:
        return QStringLiteral("info");
    }
}

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
    , grid(zone ? zone->grid_mode() : nullptr) {
    qRegisterMetaType<yodau::backend::event>("yodau::backend::event");
    qRegisterMetaType<frontend_log_entry>("frontend_log_entry");
    fps_capability = yodau::backend::detect_fps_capability_profile();
    log_buffer = new frontend_log_buffer(this);

    if (settings != nullptr) {
        settings->set_log_buffer(log_buffer);
    }

    init_from_backend();

    if (stream_mgr) {
        backend_runtime.attach(*stream_mgr);
        stream_mgr->set_event_batch_sink(
            std::bind_front(&stream_controller::on_backend_events, this)
        );
    }

    if (settings && grid) {
        settings->set_active_candidates(grid->stream_names());
        settings->set_active_current(QString());
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
    const int active_streams = active_name.isEmpty() ? 0 : 1;
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

    QStringList locals;
    const auto backend_names = stream_mgr->stream_names();
    for (const auto& n : backend_names) {
        const auto qn = QString::fromStdString(n);
        if (qn.startsWith("video")) {
            locals << qn;
        }
    }

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
            tile->set_persistent_lines(per_stream_lines.value(name));

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
        if (!active_name.isEmpty() && active_name == name && main_zone) {
            if (auto* cell = main_zone->take_active_cell()) {
                cell->deleteLater();
            }
            active_name.clear();
            if (settings) {
                settings->set_active_current(QString());
            }
        }
    }

    if (settings) {
        settings->set_active_candidates(grid->stream_names());
    }

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

void stream_controller::on_active_stream_selected(const QString& name) {
    if (!main_zone) {
        return;
    }

    active_name = name;

    if (name.isEmpty()) {
        main_zone->clear_active();
    } else {
        main_zone->set_active_stream(name);
    }

    if (auto* cell = main_zone->active_cell()) {
        cell->set_labels_enabled(active_labels_enabled);

        cell->clear_draft();
        cell->set_drawing_enabled(drawing_new_mode);

        if (drawing_new_mode) {
            cell->set_draft_params(
                draft_line_name, draft_line_color, draft_line_closed
            );
        } else if (settings) {
            apply_template_preview(settings->active_template_current());
        }
    }

    sync_active_persistent();
    refresh_fps_policy(true);
    update_monitor_inventory();

    append_log(
        frontend_log_area::active, frontend_log_severity::info,
        QStringLiteral("active_stream"),
        name.isEmpty() ? QStringLiteral("active stream cleared")
                       : QStringLiteral("active stream selected"),
        name
    );

    if (monitor_bridge != nullptr) {
        monitor_bridge->add_marker(
            name.isEmpty() ? QStringLiteral("active_stream_cleared")
                           : QStringLiteral("active_stream_selected")
        );
    }
}

void stream_controller::on_active_edit_mode_changed(bool drawing_new) {
    drawing_new_mode = drawing_new;

    if (!main_zone) {
        return;
    }

    if (auto* cell = main_zone->active_cell()) {
        cell->clear_draft();
        cell->set_drawing_enabled(drawing_new);

        if (drawing_new) {
            cell->set_draft_params(
                draft_line_name, draft_line_color, draft_line_closed
            );
        } else if (settings) {
            apply_template_preview(settings->active_template_current());
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

void stream_controller::on_active_line_params_changed(
    const QString& name, const QColor& color, bool closed
) {
    draft_line_name = name;
    draft_line_color = color;
    draft_line_closed = closed;

    if (main_zone) {
        if (auto* cell = main_zone->active_cell()) {
            cell->set_draft_params(
                draft_line_name, draft_line_color, draft_line_closed
            );
        }
    }

    append_log(
        frontend_log_area::active, frontend_log_severity::debug,
        QStringLiteral("line_editor"),
        QStringLiteral("active line draft updated"), active_name,
        QStringLiteral("name=%1 color=%2 closed=%3")
            .arg(draft_line_name)
            .arg(draft_line_color.name())
            .arg(
                draft_line_closed ? QStringLiteral("true")
                                  : QStringLiteral("false")
            )
    );
}

void stream_controller::on_active_line_save_requested(
    const QString& name, const bool closed
) {
    append_log(
        frontend_log_area::active, frontend_log_severity::debug,
        QStringLiteral("line_editor"), QStringLiteral("line save requested"),
        active_name,
        QStringLiteral("name=%1 closed=%2")
            .arg(name)
            .arg(closed ? QStringLiteral("true") : QStringLiteral("false"))
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
            QStringLiteral("line add requires at least 2 points"), active_name
        );
        return;
    }

    const auto points_str = points_str_from_pct(pts);
    append_log(
        frontend_log_area::active, frontend_log_severity::debug,
        QStringLiteral("line_editor"),
        QStringLiteral("line draft points prepared"), active_name, points_str
    );

    try {
        const auto lp = stream_mgr->add_line(
            points_str.toStdString(), closed, name.toStdString()
        );

        const auto final_name = QString::fromStdString(lp->name);
        apply_added_line(cell, final_name, pts, closed);
    } catch (const std::exception& e) {
        append_log(
            frontend_log_area::active, frontend_log_severity::error,
            QStringLiteral("line_editor"), QStringLiteral("line add failed"),
            active_name, QString::fromLocal8Bit(e.what())
        );
    }
}

void stream_controller::on_active_template_selected(
    const QString& template_name
) {
    if (drawing_new_mode) {
        return;
    }
    apply_template_preview(template_name);
}

void stream_controller::on_active_template_color_changed(const QColor& color) {
    Q_UNUSED(color);

    if (drawing_new_mode) {
        return;
    }
    if (!settings) {
        return;
    }

    const auto t = settings->active_template_current();
    if (t.isEmpty()) {
        return;
    }

    apply_template_preview(t);
}

void stream_controller::on_active_template_add_requested(
    const QString& template_name, const QColor& color
) {
    auto* cell = active_cell_checked("add template");
    if (!cell) {
        return;
    }

    if (!templates.contains(template_name)) {
        append_log(
            frontend_log_area::active, frontend_log_severity::warning,
            QStringLiteral("template_editor"),
            QStringLiteral("template add failed: unknown template"),
            active_name, template_name
        );
        return;
    }

    const auto tpl = templates.value(template_name);

    try {
        stream_mgr->set_line(
            active_name.toStdString(), template_name.toStdString()
        );
    } catch (const std::exception& e) {
        append_log(
            frontend_log_area::active, frontend_log_severity::error,
            QStringLiteral("template_editor"),
            QStringLiteral("template add failed"), active_name,
            QString::fromLocal8Bit(e.what())
        );
        return;
    }

    stream_cell::line_instance inst;
    inst.template_name = template_name;
    inst.color = color;
    inst.closed = tpl.closed;
    inst.pts_pct = tpl.pts_pct;

    per_stream_lines[active_name].push_back(inst);
    cell->add_persistent_line(inst);

    append_log(
        frontend_log_area::active, frontend_log_severity::info,
        QStringLiteral("template_editor"),
        QStringLiteral("template added to active stream"), active_name,
        template_name
    );

    cell->clear_draft();

    if (settings) {
        settings->reset_active_template_form();
    }

    sync_active_persistent();
}

void stream_controller::on_active_line_undo_requested() {
    if (!main_zone) {
        return;
    }

    auto* cell = main_zone->active_cell();
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

void stream_controller::on_active_labels_enabled_changed(bool on) {
    active_labels_enabled = on;

    if (!main_zone) {
        return;
    }

    if (auto* cell = main_zone->active_cell()) {
        cell->set_labels_enabled(active_labels_enabled);
    }
}

void stream_controller::setup_settings_connections() {
    if (!settings || !main_zone) {
        return;
    }

    connect(
        settings, &settings_panel::active_stream_selected, this,
        &stream_controller::on_active_stream_selected
    );

    connect(
        settings, &settings_panel::active_edit_mode_changed, this,
        &stream_controller::on_active_edit_mode_changed
    );

    connect(
        settings, &settings_panel::active_line_params_changed, this,
        &stream_controller::on_active_line_params_changed
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
        settings, &settings_panel::active_template_selected, this,
        &stream_controller::on_active_template_selected
    );

    connect(
        settings, &settings_panel::active_template_color_changed, this,
        &stream_controller::on_active_template_color_changed
    );

    connect(
        settings, &settings_panel::active_line_undo_requested, this,
        &stream_controller::on_active_line_undo_requested
    );

    connect(
        settings, &settings_panel::active_labels_enabled_changed, this,
        &stream_controller::on_active_labels_enabled_changed
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

    if (type == "url") {
        const QUrl url(source);
        const auto scheme = url.scheme().toLower();

        if (!url.isValid() || scheme.isEmpty()) {
            append_log(
                frontend_log_area::add, frontend_log_severity::warning,
                QStringLiteral("stream_add"), QStringLiteral("invalid url"),
                name, source
            );
            return;
        }

        if (scheme != "rtsp" && scheme != "http" && scheme != "https") {
            append_log(
                frontend_log_area::add, frontend_log_severity::warning,
                QStringLiteral("stream_add"),
                QStringLiteral("unsupported url scheme"), name, scheme
            );
            return;
        }
    }

    try {
        const auto& s = stream_mgr->add_stream(
            source.toStdString(), name.toStdString(), type.toStdString(), loop
        );

        const auto final_name = QString::fromStdString(s.get_name());
        const auto source_desc = QString("%1:%2").arg(type, source);

        QUrl url;
        if (type == "file" || type == "local") {
            url = QUrl::fromLocalFile(source);
        } else {
            url = QUrl(source);
        }
        stream_sources[final_name] = url;
        stream_loops[final_name] = loop;

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
    if (!settings) {
        return;
    }

    settings->add_existing_name(final_name);
    settings->add_stream_entry(final_name, source_desc);
    settings->clear_add_inputs();

    refresh_fps_policy(true);
    update_monitor_inventory();
}

void stream_controller::handle_enlarge_requested(const QString& name) {
    if (name.isEmpty()) {
        return;
    }

    if (!active_name.isEmpty() && active_name == name) {
        handle_back_to_grid();
        return;
    }

    on_active_stream_selected(name);

    if (settings) {
        settings->set_active_current(name);
    }
}

void stream_controller::handle_back_to_grid() {
    on_active_stream_selected(QString());

    if (settings) {
        settings->set_active_current(QString());
    }
}

void stream_controller::handle_thumb_activate(const QString& name) {
    handle_enlarge_requested(name);
}

stream_cell*
stream_controller::active_cell_checked(const QString& fail_prefix) {
    if (!stream_mgr || !main_zone || active_name.isEmpty()) {
        append_log(
            frontend_log_area::active, frontend_log_severity::warning,
            QStringLiteral("active_stream"),
            QStringLiteral("%1 failed: no active stream").arg(fail_prefix)
        );
        return nullptr;
    }

    auto* cell = main_zone->active_cell();
    if (!cell) {
        append_log(
            frontend_log_area::active, frontend_log_severity::warning,
            QStringLiteral("active_stream"),
            QStringLiteral("%1 failed: active cell not found").arg(fail_prefix),
            active_name
        );
        return nullptr;
    }

    return cell;
}

void stream_controller::sync_active_persistent() {
    if (!main_zone || active_name.isEmpty()) {
        if (settings) {
            settings->set_template_candidates({});
        }
        return;
    }

    sync_active_cell_lines();

    if (!settings) {
        return;
    }

    const auto used = used_template_names_for_stream(active_name);
    settings->set_template_candidates(template_candidates_excluding(used));
}

void stream_controller::apply_template_preview(const QString& template_name) {
    if (!main_zone) {
        return;
    }
    auto* cell = main_zone->active_cell();
    if (!cell) {
        return;
    }

    cell->clear_draft();

    if (template_name.isEmpty() || !templates.contains(template_name)) {
        return;
    }

    const auto tpl = templates.value(template_name);

    QColor c = Qt::red;
    if (settings) {
        c = settings->active_template_preview_color();
    }

    cell->set_draft_params(template_name, c, tpl.closed);
    cell->set_draft_points_pct(tpl.pts_pct);
}

void stream_controller::append_log(
    const frontend_log_area area, const frontend_log_severity severity,
    const QString& subsystem, const QString& message,
    const QString& stream_name, const QString& detail,
    const QString& algorithm_id
) const {
    frontend_log_entry entry {
        .timestamp = QDateTime(),
        .area = area,
        .severity = severity,
        .subsystem = subsystem,
        .stream_name = stream_name,
        .algorithm_id = algorithm_id,
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
        active_name, detail
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

void stream_controller::apply_added_line(
    stream_cell* cell, const QString& final_name,
    const std::vector<QPointF>& pts, const bool closed
) {
    stream_cell::line_instance inst;
    inst.template_name = final_name;
    inst.color = draft_line_color;
    inst.closed = closed;
    inst.pts_pct = pts;

    per_stream_lines[active_name].push_back(inst);
    cell->add_persistent_line(inst);

    templates[final_name] = tpl_line { pts, closed };

    stream_mgr->set_line(active_name.toStdString(), final_name.toStdString());

    cell->clear_draft();
    cell->set_draft_params(QString(), QColor(Qt::red), false);

    draft_line_name.clear();
    draft_line_color = Qt::red;
    draft_line_closed = false;

    if (settings) {
        settings->reset_active_line_form();
        settings->add_template_candidate(final_name);
        settings->reset_active_template_form();
    }

    append_log(
        frontend_log_area::active, frontend_log_severity::info,
        QStringLiteral("line_editor"), QStringLiteral("line added"),
        active_name,
        QStringLiteral("template=%1 points=%2 closed=%3")
            .arg(final_name)
            .arg(pts.size())
            .arg(closed ? QStringLiteral("true") : QStringLiteral("false"))
    );

    sync_active_persistent();
    refresh_fps_policy(true);
    update_monitor_inventory();
    if (monitor_bridge != nullptr) {
        monitor_bridge->add_marker(QStringLiteral("line_added"));
    }
}

void stream_controller::sync_active_cell_lines() const {
    if (!main_zone) {
        return;
    }

    if (auto* cell = main_zone->active_cell()) {
        cell->set_persistent_lines(per_stream_lines.value(active_name));
    }
}

QSet<QString>
stream_controller::used_template_names_for_stream(const QString& stream) const {
    QSet<QString> used;

    const auto current_lines = per_stream_lines.value(stream);
    for (const auto& inst : current_lines) {
        const auto tn = inst.template_name.trimmed();
        if (!tn.isEmpty()) {
            used.insert(tn);
        }
    }

    return used;
}

QStringList stream_controller::template_candidates_excluding(
    const QSet<QString>& used
) const {
    QStringList candidates;
    candidates.reserve(templates.size());

    for (auto it = templates.begin(); it != templates.end(); ++it) {
        const QString name = it.key();
        if (!used.contains(name)) {
            candidates << name;
        }
    }

    return candidates;
}

void stream_controller::refresh_fps_policy(const bool force) {
    if (!stream_mgr || !grid) {
        return;
    }

    const auto now = stream_controller_support::steady_clock::now();
    stream_controller_support::prune_expired_motion_events(
        recent_motion_events, now
    );

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
        + (active_name.isEmpty() ? 0 : 1);
    const int active_stream_count = active_name.isEmpty() ? 0 : 1;
    const int cell_count = grid_cell_count();
    const int motion_count = recent_motion_count();
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
            if (auto* cell = main_zone->active_cell()) {
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

int stream_controller::recent_motion_count() {
    const auto now = stream_controller_support::steady_clock::now();
    stream_controller_support::prune_expired_motion_events(
        recent_motion_events, now
    );
    return static_cast<int>(recent_motion_events.size());
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

stream_cell*
stream_controller::tile_for_stream_name(const QString& name) const {
    if (main_zone && !active_name.isEmpty() && active_name == name) {
        if (auto* cell = main_zone->active_cell()) {
            return cell;
        }
    }

    if (grid) {
        if (auto* tile = grid->peek_stream_cell(name)) {
            return tile;
        }
    }

    return nullptr;
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
    const auto name = QString::fromStdString(event_value.stream_name);
    const QString line_name = QString::fromStdString(event_value.line_name);
    QString event_detail;

    if (!line_name.isEmpty()) {
        event_detail = QStringLiteral("line=%1").arg(line_name);
    }

    if (event_value.pos_pct.has_value()) {
        const auto& position = *event_value.pos_pct;
        const auto position_text = QStringLiteral("pos=(%1,%2)")
                                       .arg(position.x, 0, 'f', 3)
                                       .arg(position.y, 0, 'f', 3);
        if (!event_detail.isEmpty()) {
            event_detail.append(' ');
        }
        event_detail.append(position_text);
    }

    const auto backend_message
        = QString::fromStdString(event_value.message).trimmed();
    if (!backend_message.isEmpty()) {
        if (!event_detail.isEmpty()) {
            event_detail.append(' ');
        }
        event_detail.append(QStringLiteral("backend=%1").arg(backend_message));
    }

    QString log_message = QStringLiteral("backend event");
    frontend_log_severity log_severity = frontend_log_severity::info;

    switch (event_value.kind) {
    case yodau::backend::event_kind::motion:
        log_message = QStringLiteral("motion detected");
        log_severity = frontend_log_severity::debug;
        break;
    case yodau::backend::event_kind::tripwire:
        log_message = QStringLiteral("tripwire triggered");
        break;
    case yodau::backend::event_kind::roi:
        log_message = QStringLiteral("roi event");
        break;
    case yodau::backend::event_kind::info:
    default:
        log_message = QStringLiteral("backend info event");
        break;
    }

    append_log(
        frontend_log_area::active, log_severity,
        QStringLiteral("backend_event"), log_message, name, event_detail
    );

    emit monitor_backend_event_observed(
        backend_event_kind_text(event_value.kind)
    );
    auto* tile = tile_for_stream_name(name);
    if (!tile) {
        return;
    }

    if (!event_value.pos_pct.has_value()) {
        return;
    }

    if (event_value.kind == yodau::backend::event_kind::tripwire) {
        if (!event_value.line_name.empty()) {
            const auto ln = QString::fromStdString(event_value.line_name);
            const auto& p = *event_value.pos_pct;
            const auto payload
                = stream_controller_support::parse_tripwire_visual_payload(
                    event_value.message
                );

            tile->highlight_line_at(
                ln, QPointF(p.x, p.y), payload.strength, payload.direction,
                payload.speed
            );
        }
    }

    bool allow_gui_motion = true;
    if (event_value.kind == yodau::backend::event_kind::motion) {
        recent_motion_events.push_back(
            stream_controller_support::steady_clock::now()
        );
        const auto now = QDateTime::currentDateTime();
        if (last_gui_motion_event_ts.contains(name)) {
            const int age
                = static_cast<int>(last_gui_motion_event_ts[name].msecsTo(now));
            if (age < motion_gui_interval_ms) {
                allow_gui_motion = false;
            }
        }
        if (allow_gui_motion) {
            last_gui_motion_event_ts[name] = now;
        }
        refresh_fps_policy(false);
    }

    if (!allow_gui_motion) {
        return;
    }

    const auto& p = *event_value.pos_pct;
    tile->add_event(QPointF(p.x, p.y), Qt::gray);
}
