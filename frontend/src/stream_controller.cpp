#include "stream_controller.hpp"
#include "helpers/str_label.hpp"
#include "stream.hpp"
#include "widgets/settings_panel.hpp"

#include <QDateTime>

stream_controller::stream_controller(
    yodau::backend::stream_manager* mgr, settings_panel* panel, QObject* parent
)
    : QObject(parent)
    , stream_mgr(mgr)
    , settings(panel) {
    init_from_backend();
}

void stream_controller::init_from_backend() const {
    QSet<QString> names;

    const auto backend_names = stream_mgr->stream_names();
    for (auto& n : backend_names) {
        auto qname = QString::fromStdString(n);
        names.insert(qname);

        settings->add_stream_entry(qname, str_label("<unknown>"));
    }

    settings->set_existing_names(names);
}

void stream_controller::handle_add_file(
    const QString& path, const QString& name, const bool loop
) const {
    auto ts = now_ts();

    try {
        const auto& s = stream_mgr->add_stream(
            path.toStdString(), name.toStdString(), "file", loop
        );

        auto final_name = QString::fromStdString(s.get_name());
        auto source_desc = QString("file:%1").arg(path);

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

void stream_controller::handle_add_local(
    const QString& source, const QString& name
) const {
    auto ts = now_ts();

    try {
        const auto& s = stream_mgr->add_stream(
            source.toStdString(), name.toStdString(), "local", true
        );

        auto final_name = QString::fromStdString(s.get_name());
        auto source_desc = QString("local:%1").arg(source);

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

void stream_controller::handle_add_url(
    const QString& url, const QString& name
) const {
    auto ts = now_ts();

    try {
        const auto& s = stream_mgr->add_stream(
            url.toStdString(), name.toStdString(), "url", true
        );

        auto final_name = QString::fromStdString(s.get_name());
        auto source_desc = QString("url:%1").arg(url);

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

void stream_controller::handle_detect_local_sources() const {
    const auto ts = now_ts();
    stream_mgr->refresh_local_streams();
    settings->append_add_log(
        QString("[%1] detect local sources requested").arg(ts)
    );
}

void stream_controller::handle_show_stream_changed(
    const QString& name, const bool show
) {
    Q_UNUSED(name);
    Q_UNUSED(show);
}

void stream_controller::handle_backend_event(const QString& text) const {
    settings->append_event(text);
}

QString stream_controller::now_ts() {
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

void stream_controller::register_stream_in_ui(
    const QString& final_name, const QString& source_desc
) const {
    settings->add_existing_name(final_name);
    settings->add_stream_entry(final_name, source_desc);
    settings->clear_add_inputs();
}
