#include "helpers/controller.hpp"
#include "geometry.hpp"
#include "helpers/str_label.hpp"
#include "stream.hpp"
#include "widgets/settings_panel.hpp"

#include <QCameraDevice>
#include <QColor>
#include <QDateTime>
#include <QDebug>
#include <QMediaDevices>
#include <QMetaObject>
#include <QThread>
#include <QtGlobal>

#include "event.hpp"
#include "frame.hpp"
#include "widgets/board.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/stream_cell.hpp"

controller::controller(
    yodau::backend::stream_manager* mgr, settings_panel* panel, board* zone,
    QObject* parent
)
    : QObject(parent)
    , stream_mgr(mgr)
    , settings(panel)
    , main_zone(zone)
    , grid(zone ? zone->grid_mode() : nullptr) {

    init_from_backend();

    if (stream_mgr) {
        stream_mgr->set_frame_processor([this](
                                            const yodau::backend::stream& s,
                                            const yodau::backend::frame& f
                                        ) { return make_fake_events(s, f); });

        stream_mgr->enable_fake_events(700);

        stream_mgr->set_event_batch_sink(
            [this](const std::vector<yodau::backend::event>& evs) {
                on_backend_events(evs);
            }
        );
    }

    if (settings && grid) {
        settings->set_active_candidates(grid->stream_names());
        settings->set_active_current(QString());
    }

    setup_settings_connections();
    setup_grid_connections();
}

void controller::init_from_backend() {
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

void controller::handle_add_file(
    const QString& path, const QString& name, const bool loop
) {
    handle_add_stream_common(path, name, "file", loop);
}

void controller::handle_add_local(const QString& source, const QString& name) {
    handle_add_stream_common(source, name, "local", true);
}

void controller::handle_add_url(const QString& url, const QString& name) {
    handle_add_stream_common(url, name, "url", true);
}

void controller::handle_detect_local_sources() {
    if (!stream_mgr || !settings) {
        return;
    }

    const auto ts = now_ts();
    stream_mgr->refresh_local_streams();

    QStringList locals;
    const auto backend_names = stream_mgr->stream_names();
    for (const auto& n : backend_names) {
        const auto qn = QString::fromStdString(n);
        if (qn.startsWith("video")) {
            locals << qn;
        }
    }

    settings->set_local_sources(locals);
    settings->append_add_log(
        QString("[%1] ok: detected %2 local sources").arg(ts).arg(locals.size())
    );

    const auto cams = QMediaDevices::videoInputs();
    // for (int i = 0; i < cams.size(); ++i) {
    //     const auto& c = cams[i];
    // }
}

void controller::handle_show_stream_changed(
    const QString& name, const bool show
) {
    if (!grid) {
        return;
    }

    if (show) {
        grid->add_stream(name);
        if (auto* tile = grid->peek_stream_cell(name)) {
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

    update_repaint_caps();
}

void controller::handle_backend_event(const QString& text) {
    if (settings) {
        settings->append_active_log(text);
    }
}

void controller::on_active_stream_selected(const QString& name) {
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
    update_repaint_caps();
}

void controller::on_active_edit_mode_changed(bool drawing_new) {
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

    if (settings) {
        settings->append_active_log(
            QString("edit mode: %1")
                .arg(drawing_new ? "draw new" : "use template")
        );
    }
}

void controller::on_active_line_params_changed(
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

    if (settings) {
        settings->append_active_log(
            QString("active line params: name='%1' color=%2 closed=%3")
                .arg(draft_line_name)
                .arg(draft_line_color.name())
                .arg(draft_line_closed ? "true" : "false")
        );
    }
}

void controller::on_active_line_save_requested(
    const QString& name, const bool closed
) {
    log_active(QString("save click: name='%1' closed=%2 active='%3'")
                   .arg(name)
                   .arg(closed ? "true" : "false")
                   .arg(active_name));

    auto* cell = active_cell_checked("add line");
    if (!cell) {
        return;
    }

    const auto pts = cell->draft_points_pct();
    if (pts.size() < 2) {
        log_active("add line failed: need at least 2 points");
        return;
    }

    const auto points_str = points_str_from_pct(pts);
    log_active(QString("points_str = %1").arg(points_str));

    try {
        const auto lp = stream_mgr->add_line(
            points_str.toStdString(), closed, name.toStdString()
        );

        const auto final_name = QString::fromStdString(lp->name);
        apply_added_line(cell, final_name, pts, closed);
    } catch (const std::exception& e) {
        log_active(QString("add line failed: %1").arg(e.what()));
    }
}

void controller::on_active_template_selected(const QString& template_name) {
    if (drawing_new_mode) {
        return;
    }
    apply_template_preview(template_name);
}

void controller::on_active_template_color_changed(const QColor& color) {
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

void controller::on_active_template_add_requested(
    const QString& template_name, const QColor& color
) {
    auto* cell = active_cell_checked("add template");
    if (!cell) {
        return;
    }

    if (!templates.contains(template_name)) {
        if (settings) {
            settings->append_active_log(
                QString("add template failed: unknown template '%1'")
                    .arg(template_name)
            );
        }
        return;
    }

    const auto tpl = templates.value(template_name);

    try {
        stream_mgr->set_line(
            active_name.toStdString(), template_name.toStdString()
        );
    } catch (const std::exception& e) {
        if (settings) {
            settings->append_active_log(
                QString("add template failed: %1").arg(e.what())
            );
        }
        return;
    }

    stream_cell::line_instance inst;
    inst.template_name = template_name;
    inst.color = color;
    inst.closed = tpl.closed;
    inst.pts_pct = tpl.pts_pct;

    per_stream_lines[active_name].push_back(inst);
    cell->add_persistent_line(inst);

    if (settings) {
        settings->append_active_log(
            QString("template added to active: %1").arg(template_name)
        );
    }

    cell->clear_draft();

    if (settings) {
        settings->reset_active_template_form();
    }

    sync_active_persistent();
}

void controller::on_active_line_undo_requested() {
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

void controller::on_active_labels_enabled_changed(bool on) {
    active_labels_enabled = on;

    if (!main_zone) {
        return;
    }

    if (auto* cell = main_zone->active_cell()) {
        cell->set_labels_enabled(active_labels_enabled);
    }
}

void controller::setup_settings_connections() {
    if (!settings || !main_zone) {
        return;
    }

    connect(
        settings, &settings_panel::active_stream_selected, this,
        &controller::on_active_stream_selected
    );

    connect(
        settings, &settings_panel::active_edit_mode_changed, this,
        &controller::on_active_edit_mode_changed
    );

    connect(
        settings, &settings_panel::active_line_params_changed, this,
        &controller::on_active_line_params_changed
    );

    connect(
        settings, &settings_panel::active_line_save_requested, this,
        &controller::on_active_line_save_requested
    );

    connect(
        settings, &settings_panel::active_template_add_requested, this,
        &controller::on_active_template_add_requested
    );

    connect(
        settings, &settings_panel::active_template_selected, this,
        &controller::on_active_template_selected
    );

    connect(
        settings, &settings_panel::active_template_color_changed, this,
        &controller::on_active_template_color_changed
    );

    connect(
        settings, &settings_panel::active_line_undo_requested, this,
        &controller::on_active_line_undo_requested
    );

    connect(
        settings, &settings_panel::active_labels_enabled_changed, this,
        &controller::on_active_labels_enabled_changed
    );
}

void controller::setup_grid_connections() {
    if (!grid) {
        return;
    }

    connect(grid, &grid_view::stream_closed, this, [this](const QString& name) {
        if (settings) {
            settings->set_stream_checked(name, false);
        }
    });

    connect(
        grid, &grid_view::stream_enlarge, this,
        &controller::handle_enlarge_requested
    );
}

void controller::handle_add_stream_common(
    const QString& source, const QString& name, const QString& type, bool loop
) {
    if (!stream_mgr || !settings) {
        return;
    }

    const auto ts = now_ts();

    if (type == "url") {
        const QUrl url(source);
        const auto scheme = url.scheme().toLower();

        if (!url.isValid() || scheme.isEmpty()) {
            settings->append_add_log(
                QString("[%1] error: invalid url '%2'").arg(ts, source)
            );
            return;
        }

        if (scheme != "rtsp" && scheme != "http" && scheme != "https") {
            settings->append_add_log(
                QString("[%1] error: unsupported url scheme '%2'")
                    .arg(ts, scheme)
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

        settings->append_add_log(
            QString("[%1] ok: added %2 as %3").arg(ts, source_desc, final_name)
        );

        register_stream_in_ui(final_name, source_desc);
    } catch (const std::exception& e) {
        settings->append_add_log(
            QString("[%1] error: add %2 failed: %3").arg(ts, type, e.what())
        );
    }
}

void controller::register_stream_in_ui(
    const QString& final_name, const QString& source_desc
) {
    if (!settings) {
        return;
    }

    settings->add_existing_name(final_name);
    settings->add_stream_entry(final_name, source_desc);
    settings->clear_add_inputs();

    update_repaint_caps();
}

QString controller::now_ts() {
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

void controller::handle_enlarge_requested(const QString& name) {
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

void controller::handle_back_to_grid() {
    on_active_stream_selected(QString());

    if (settings) {
        settings->set_active_current(QString());
    }
}

void controller::handle_thumb_activate(const QString& name) {
    handle_enlarge_requested(name);
}

stream_cell* controller::active_cell_checked(const QString& fail_prefix) {
    if (!stream_mgr || !main_zone || active_name.isEmpty()) {
        if (settings) {
            settings->append_active_log(
                QString("%1 failed: no active stream").arg(fail_prefix)
            );
        }
        return nullptr;
    }

    auto* cell = main_zone->active_cell();
    if (!cell) {
        if (settings) {
            settings->append_active_log(
                QString("%1 failed: active cell not found").arg(fail_prefix)
            );
        }
        return nullptr;
    }

    return cell;
}

void controller::sync_active_persistent() {
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

void controller::apply_template_preview(const QString& template_name) {
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

void controller::log_active(const QString& msg) const {
    if (settings) {
        settings->append_active_log(msg);
    }
}

QString controller::points_str_from_pct(const std::vector<QPointF>& pts) {
    QStringList parts;
    parts.reserve(static_cast<int>(pts.size()));
    for (const auto& p : pts) {
        parts << QString("(%1,%2)").arg(p.x(), 0, 'f', 3).arg(p.y(), 0, 'f', 3);
    }
    return parts.join("; ");
}

void controller::apply_added_line(
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

    log_active(
        QString("line added: %1 (%2 points)").arg(final_name).arg(pts.size())
    );

    sync_active_persistent();
}

void controller::sync_active_cell_lines() const {
    if (!main_zone) {
        return;
    }

    if (auto* cell = main_zone->active_cell()) {
        cell->set_persistent_lines(per_stream_lines.value(active_name));
    }
}

QSet<QString>
controller::used_template_names_for_stream(const QString& stream) const {
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

QStringList
controller::template_candidates_excluding(const QSet<QString>& used) const {
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

void controller::update_repaint_caps() {
    if (!grid) {
        return;
    }

    const auto names = grid->stream_names();
    const int n = static_cast<int>(names.size());

    int interval = 66;
    if (n <= 2) {
        interval = 33; // ~30 fps
    } else if (n <= 4) {
        interval = 66; // ~15 fps
    } else if (n <= 9) {
        interval = 100; // ~10 fps
    } else {
        interval = 166; // ~6 fps
    }

    for (const auto& name : names) {
        if (auto* tile = grid->peek_stream_cell(name)) {
            tile->set_repaint_interval_ms(interval);
        }
    }

    if (!active_name.isEmpty()) {
        if (auto* cell = grid->peek_stream_cell(active_name)) {
            cell->set_repaint_interval_ms(active_interval_ms);
        }
    }
}

std::vector<yodau::backend::event> controller::make_fake_events(
    const yodau::backend::stream& s, const yodau::backend::frame& f
) const {
    Q_UNUSED(f);

    std::vector<yodau::backend::event> out;

    const int chance = QRandomGenerator::global()->bounded(100);
    if (chance > 12) {
        return out;
    }

    yodau::backend::event e;
    e.ts = std::chrono::steady_clock::now();
    e.stream_name = s.get_name();

    const double x = QRandomGenerator::global()->bounded(5, 95);
    const double y = QRandomGenerator::global()->bounded(5, 95);
    e.pos_pct = yodau::backend::point { static_cast<float>(x),
                                        static_cast<float>(y) };

    const int k = QRandomGenerator::global()->bounded(2);
    e.kind = k == 0 ? yodau::backend::event_kind::motion
                    : yodau::backend::event_kind::tripwire;

    const auto ln = s.line_names();
    if (!ln.empty() && e.kind == yodau::backend::event_kind::tripwire) {
        const int li
            = QRandomGenerator::global()->bounded(static_cast<int>(ln.size()));
        e.line_name = ln[static_cast<size_t>(li)];
    }

    out.push_back(std::move(e));
    return out;
}

void controller::on_backend_events(
    const std::vector<yodau::backend::event>& evs
) {
    for (const auto& e : evs) {
        on_backend_event(e);
    }
}

void controller::on_backend_event(const yodau::backend::event& e) {
    if (QThread::currentThread() != thread()) {
        const auto copy = e;
        QMetaObject::invokeMethod(
            this, [this, copy]() { on_backend_event(copy); },
            Qt::QueuedConnection
        );
        return;
    }

    const auto name = QString::fromStdString(e.stream_name);

    stream_cell* tile = nullptr;

    if (main_zone && !active_name.isEmpty() && active_name == name) {
        tile = main_zone->active_cell();
    }

    if (!tile && grid) {
        tile = grid->peek_stream_cell(name);
    }

    if (!tile) {
        return;
    }

    if (!e.pos_pct.has_value()) {
        return;
    }

    if (e.kind == yodau::backend::event_kind::tripwire) {
        if (!e.line_name.empty()) {
            tile->highlight_line(QString::fromStdString(e.line_name));
        }
    }

    const auto& p = *e.pos_pct;

    tile->add_event(QPointF(p.x, p.y), Qt::gray);
}
