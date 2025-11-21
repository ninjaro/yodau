#include "helpers/controller.hpp"
#include "helpers/str_label.hpp"
#include "stream.hpp"
#include "widgets/settings_panel.hpp"

#include <QDateTime>
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
