#ifndef YODAU_FRONTEND_STREAM_CONTROLLER_HPP
#define YODAU_FRONTEND_STREAM_CONTROLLER_HPP

#include <QObject>
#include <QString>

#include "stream_manager.hpp"

class settings_panel;

class stream_controller final : public QObject {
    Q_OBJECT
public:
    explicit stream_controller(
        yodau::backend::stream_manager* mgr, settings_panel* panel,
        QObject* parent = nullptr
    );

    void init_from_backend() const;

public slots:
    void
    handle_add_file(const QString& path, const QString& name, bool loop) const;
    void handle_add_local(const QString& source, const QString& name) const;
    void handle_add_url(const QString& url, const QString& name) const;

    void handle_detect_local_sources() const;
    void handle_show_stream_changed(const QString& name, bool show);

    void handle_backend_event(const QString& text) const;

private:
    static QString now_ts();
    void register_stream_in_ui(
        const QString& final_name, const QString& source_desc
    ) const;

    yodau::backend::stream_manager* stream_mgr;
    settings_panel* settings;
};

#endif // YODAU_FRONTEND_STREAM_CONTROLLER_HPP