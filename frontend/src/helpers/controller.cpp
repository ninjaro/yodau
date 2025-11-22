#include "helpers/controller.hpp"
#include "geometry.hpp"
#include "helpers/str_label.hpp"
#include "stream.hpp"
#include "widgets/settings_panel.hpp"

#include <QColor>
#include <QDateTime>
#include <QDebug>
#include <QtGlobal>

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

        settings->add_stream_entry(qname, str_label("<unknown>"));
    }

    settings->set_existing_names(names);
}

void controller::handle_add_file(
    const QString& path, const QString& name, const bool loop
) {
    if (!stream_mgr || !settings) {
        return;
    }

    const auto ts = now_ts();

    try {
        const auto& s = stream_mgr->add_stream(
            path.toStdString(), name.toStdString(), "file", loop
        );

        const auto final_name = QString::fromStdString(s.get_name());
        const auto source_desc = QString("file:%1").arg(path);

        settings->append_add_log(
            QString("[%1] ok: added %2 as %3").arg(ts, source_desc, final_name)
        );

        register_stream_in_ui(final_name, source_desc);
    } catch (const std::exception& e) {
        settings->append_add_log(
            QString("[%1] error: add file failed: %2").arg(ts, e.what())
        );
    }
}

void controller::handle_add_local(const QString& source, const QString& name) {
    if (!stream_mgr || !settings) {
        return;
    }

    const auto ts = now_ts();

    try {
        const auto& s = stream_mgr->add_stream(
            source.toStdString(), name.toStdString(), "local", true
        );

        const auto final_name = QString::fromStdString(s.get_name());
        const auto source_desc = QString("local:%1").arg(source);

        settings->append_add_log(
            QString("[%1] ok: added %2 as %3").arg(ts, source_desc, final_name)
        );

        register_stream_in_ui(final_name, source_desc);
    } catch (const std::exception& e) {
        settings->append_add_log(
            QString("[%1] error: add local failed: %2").arg(ts, e.what())
        );
    }
}

void controller::handle_add_url(const QString& url, const QString& name) {
    if (!stream_mgr || !settings) {
        return;
    }

    const auto ts = now_ts();

    try {
        const auto& s = stream_mgr->add_stream(
            url.toStdString(), name.toStdString(), "url", true
        );

        const auto final_name = QString::fromStdString(s.get_name());
        const auto source_desc = QString("url:%1").arg(url);

        settings->append_add_log(
            QString("[%1] ok: added %2 as %3").arg(ts, source_desc, final_name)
        );

        register_stream_in_ui(final_name, source_desc);
    } catch (const std::exception& e) {
        settings->append_add_log(
            QString("[%1] error: add url failed: %2").arg(ts, e.what())
        );
    }
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
}

void controller::handle_show_stream_changed(
    const QString& name, const bool show
) {
    if (!grid) {
        return;
    }

    if (show) {
        grid->add_stream(name);
    } else {
        grid->remove_stream(name);
        // active_name.clear();
        if (!active_name.isEmpty() && active_name == name && main_zone) {
            if (auto* cell = main_zone->take_active_cell()) {
                cell->deleteLater();
            }
            active_name.clear();
            if (settings) {
                settings->set_active_current(QString());
            }
        }
        // grid->set_active_stream(QString());
    }

    if (settings) {
        settings->set_active_candidates(grid->stream_names());
    }
}

void controller::handle_backend_event(const QString& text) {
    if (settings) {
        settings->append_event(text);
    }
}

void controller::handle_enlarge_requested(const QString& name) {
    if (!grid || !main_zone) {
        return;
    }

    if (!active_name.isEmpty() && active_name == name) {
        handle_back_to_grid();
        return;
    }

    active_name = name;

    main_zone->set_active_stream(name);
    sync_active_persistent();

    if (settings) {
        settings->set_active_current(name);
    }

    // grid->set_active_stream(name);
}

void controller::handle_back_to_grid() {
    active_name.clear();

    if (main_zone) {
        main_zone->clear_active();
    }

    if (settings) {
        settings->set_active_current(QString());
    }

    // if (grid) {
    //     grid->set_active_stream(QString());
    // }
}

void controller::handle_thumb_activate(const QString& name) {
    handle_enlarge_requested(name);
}

void controller::sync_active_persistent() {
    if (!main_zone || active_name.isEmpty()) {
        if (settings) {
            settings->set_template_candidates({});
        }
        return;
    }

    if (auto* cell = main_zone->active_cell()) {
        cell->set_persistent_lines(per_stream_lines.value(active_name));
    }

    if (!settings) {
        return;
    }

    QSet<QString> used;
    const auto current_lines = per_stream_lines.value(active_name);
    for (const auto& inst : current_lines) {
        const auto tn = inst.template_name.trimmed();
        if (!tn.isEmpty()) {
            used.insert(tn);
        }
    }

    QStringList candidates;
    candidates.reserve(templates.size());
    for (auto it = templates.begin(); it != templates.end(); ++it) {
        const QString name = it.key();
        if (!used.contains(name)) {
            candidates << name;
        }
    }

    settings->set_template_candidates(candidates);
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

QString controller::now_ts() {
    return QDateTime::currentDateTime().toString("HH:mm:ss");
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
    }

    sync_active_persistent();
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
        settings->append_event(
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
        settings->append_event(
            QString("active line params: name='%1' color=%2 closed=%3")
                .arg(draft_line_name)
                .arg(draft_line_color.name())
                .arg(draft_line_closed ? "true" : "false")
        );
    }
}

void controller::on_active_template_selected(const QString& template_name) {
    if (drawing_new_mode) {
        return;
    }
    apply_template_preview(template_name);
}

void controller::on_active_line_save_requested(
    const QString& name, const bool closed
) {
    qDebug() << "SAVE CLICK"
             << "name=" << name << "closed=" << closed
             << "active_name=" << active_name
             << "mgr=" << (stream_mgr != nullptr)
             << "zone=" << (main_zone != nullptr);

    if (settings) {
        settings->append_event(
            QString("save click: name='%1' closed=%2 active='%3'")
                .arg(name)
                .arg(closed ? "true" : "false")
                .arg(active_name)
        );
    }

    if (!stream_mgr || !main_zone || active_name.isEmpty()) {
        if (settings) {
            settings->append_event("add line failed: no active stream");
        }
        return;
    }

    auto* cell = main_zone->active_cell();
    qDebug() << "active cell=" << (cell != nullptr);

    if (cell) {
        qDebug() << "draft pts=" << cell->draft_points_pct().size();
    }
    if (!cell) {
        if (settings) {
            settings->append_event("add line failed: active cell not found");
        }
        return;
    }

    const auto pts = cell->draft_points_pct();
    if (pts.size() < 2) {
        if (settings) {
            settings->append_event("add line failed: need at least 2 points");
        }
        return;
    }

    QStringList parts;
    parts.reserve(int(pts.size()));
    for (const auto& p : pts) {
        parts << QString("(%1,%2)").arg(p.x(), 0, 'f', 3).arg(p.y(), 0, 'f', 3);
    }
    const auto points_str = parts.join("; ");

    qDebug() << "points_str (qstring) =" << points_str;
    qDebug() << "points_str bytes =" << points_str.toUtf8();
    if (settings) {
        settings->append_event(QString("points_str = %1").arg(points_str));
    }

    try {
        qDebug() << "calling add_line...";
        const auto lp = stream_mgr->add_line(
            points_str.toStdString(), closed, name.toStdString()
        );
        qDebug() << "add_line ok, lp=" << (lp != nullptr);

        const auto final_name = QString::fromStdString(lp->name);
        qDebug() << "final_name =" << final_name;

        stream_cell::line_instance inst;
        inst.template_name = final_name;
        inst.color = draft_line_color;
        inst.closed = closed;
        inst.pts_pct = pts; // todo use lp->points

        per_stream_lines[active_name].push_back(inst);
        cell->add_persistent_line(inst);

        templates[final_name] = tpl_line { pts, closed };

        qDebug() << "calling set_line...";
        stream_mgr->set_line(
            active_name.toStdString(), final_name.toStdString()
        );
        qDebug() << "set_line ok";

        cell->clear_draft();
        cell->set_draft_params(QString(), QColor(Qt::red), false);

        draft_line_name.clear();
        draft_line_color = Qt::red;
        draft_line_closed = false;

        if (settings) {
            settings->reset_active_line_form();
        }

        if (settings) {
            settings->add_template_candidate(final_name);
            settings->reset_active_template_form();
            settings->append_event(QString("line added: %1 (%2 points)")
                                       .arg(final_name)
                                       .arg(pts.size()));
        }
        sync_active_persistent();
    } catch (const std::exception& e) {
        qDebug() << "EXCEPTION:" << e.what();
        if (settings) {
            settings->append_event(
                QString("add line failed: %1").arg(e.what())
            );
        }
    }
}

void controller::on_active_template_add_requested(
    const QString& template_name, const QColor& color
) {
    if (!stream_mgr || !main_zone || active_name.isEmpty()) {
        if (settings) {
            settings->append_event("add template failed: no active stream");
        }
        return;
    }

    if (!templates.contains(template_name)) {
        if (settings) {
            settings->append_event(
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
            settings->append_event(
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

    if (auto* cell2 = main_zone->active_cell()) {
        cell2->add_persistent_line(inst);
    }

    if (settings) {
        settings->append_event(
            QString("template added to active: %1").arg(template_name)
        );
    }

    if (auto* cell2 = main_zone->active_cell()) {
        cell2->clear_draft();
    }

    if (settings) {
        settings->reset_active_template_form();
    }

    sync_active_persistent();
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
