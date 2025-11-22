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
    // if (main_zone) {
    //     // connect(
    //     //     main_zone, &board::active_shrink_requested, this,
    //     //     [this](const QString&) { handle_back_to_grid(); }
    //     // );
    //     connect(
    //         main_zone, &board::active_close_requested, this,
    //         [this](const QString& name) {
    //             if (settings) {
    //                 settings->set_stream_checked(name, false);
    //             } else if (main_zone) {
    //                 main_zone->clear_active();
    //             }
    //         }
    //     );
    // }
    if (settings && main_zone) {
        connect(
            settings, &settings_panel::active_stream_selected, this,
            [this](const QString& name) {
                if (!main_zone) {
                    return;
                }
                active_name = name;
                if (name.isEmpty()) {
                    main_zone->clear_active();
                } else {
                    main_zone->set_active_stream(name);
                }
                // if (grid) {
                //     grid->set_active_stream(name);
                // }
            }
        );
        // if (settings) {
        connect(
            settings, &settings_panel::active_edit_mode_changed, this,
            [this](bool drawing_new) {
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
                    }
                }
                if (settings) {
                    settings->append_event(
                        QString("edit mode: %1")
                            .arg(drawing_new ? "draw new" : "use template")
                    );
                }
            }
        );

        connect(
            settings, &settings_panel::active_line_params_changed, this,
            [this](const QString& name, const QColor& color, bool closed) {
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
                        QString(
                            "active line params: name='%1' color=%2 "
                            "closed=%3"
                        )
                            .arg(draft_line_name)
                            .arg(draft_line_color.name())
                            .arg(draft_line_closed ? "true" : "false")
                    );
                }
            }

        );

        connect(
            settings, &settings_panel::active_line_save_requested, this,
            [this](const QString& name, bool closed) {
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
                        settings->append_event(
                            "add line failed: no active stream"
                        );
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
                        settings->append_event(
                            "add line failed: active cell not found"
                        );
                    }
                    return;
                }

                const auto pts = cell->draft_points_pct();
                if (pts.size() < 2) {
                    if (settings) {
                        settings->append_event(
                            "add line failed: need at least 2 points"
                        );
                    }
                    return;
                }

                QStringList parts;
                parts.reserve(int(pts.size()));
                for (const auto& p : pts) {
                    parts << QString("(%1,%2)")
                                 .arg(p.x(), 0, 'f', 3)
                                 .arg(p.y(), 0, 'f', 3);
                }
                const auto points_str = parts.join("; ");

                qDebug() << "points_str (qstring) =" << points_str;
                qDebug() << "points_str bytes =" << points_str.toUtf8();
                if (settings) {
                    settings->append_event(
                        QString("points_str = %1").arg(points_str)
                    );
                }

                try {
                    qDebug() << "calling add_line...";
                    const auto lp = stream_mgr->add_line(
                        points_str.toStdString(), closed, name.toStdString()
                    );
                    qDebug() << "add_line ok, lp=" << (lp != nullptr);

                    const auto final_name = QString::fromStdString(lp->name);
                    qDebug() << "final_name =" << final_name;

                    templates[final_name] = tpl_line { pts, closed };

                    qDebug() << "calling set_line...";
                    stream_mgr->set_line(
                        active_name.toStdString(), final_name.toStdString()
                    );
                    qDebug() << "set_line ok";
                    if (auto* cell2 = main_zone->active_cell()) {
                        cell2->clear_draft();
                        cell2->set_draft_params(
                            QString(), draft_line_color, false
                        );
                    }

                    if (settings) {
                        settings->add_template_candidate(final_name);
                        settings->append_event(
                            QString("line added: %1 (%2 points)")
                                .arg(final_name)
                                .arg(pts.size())
                        );
                    }
                } catch (const std::exception& e) {
                    qDebug() << "EXCEPTION:" << e.what();
                    if (settings) {
                        settings->append_event(
                            QString("add line failed: %1").arg(e.what())
                        );
                    }
                }
            }
        );

        connect(
            settings, &settings_panel::active_template_add_requested, this,
            [this](const QString& template_name, const QColor& color) {
                if (!stream_mgr || !main_zone || active_name.isEmpty()) {
                    if (settings) {
                        settings->append_event(
                            "add template failed: no active stream"
                        );
                    }
                    return;
                }

                if (!templates.contains(template_name)) {
                    if (settings) {
                        settings->append_event(
                            QString(
                                "add template failed: unknown template '%1'"
                            )
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

                if (auto* cell = main_zone->active_cell()) {
                    cell->clear_draft();
                    cell->set_draft_params(template_name, color, tpl.closed);
                    cell->set_draft_points_pct(tpl.pts_pct);
                }

                if (settings) {
                    settings->append_event(
                        QString("template added to active: %1")
                            .arg(template_name)
                    );
                }
            }
        );
        // }
    }

    if (grid) {
        connect(
            grid, &grid_view::stream_closed, this, [this](const QString& name) {
                if (settings) {
                    settings->set_stream_checked(name, false);
                }
            }
        );

        connect(
            grid, &grid_view::stream_enlarge, this,
            &controller::handle_enlarge_requested
        );
    }
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
