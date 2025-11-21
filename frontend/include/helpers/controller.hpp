#ifndef YODAU_FRONTEND_HELPERS_CONTROLLER_HPP
#define YODAU_FRONTEND_HELPERS_CONTROLLER_HPP

#include <QObject>
#include <QString>

#include "stream_manager.hpp"

class settings_panel;
class board;
class grid_view;

class controller final : public QObject {
    Q_OBJECT

public:
    explicit controller(
        yodau::backend::stream_manager* mgr, settings_panel* panel, board* zone,
        QObject* parent = nullptr
    );

    void init_from_backend();

public slots:
    void handle_add_file(const QString& path, const QString& name, bool loop);
    void handle_add_local(const QString& source, const QString& name);
    void handle_add_url(const QString& url, const QString& name);
    void handle_detect_local_sources();
    void handle_show_stream_changed(const QString& name, bool show);
    void handle_backend_event(const QString& text);

private:
    void handle_enlarge_requested(const QString& name);
    void handle_back_to_grid();
    void handle_thumb_activate(const QString& name);

    static QString now_ts();
    void register_stream_in_ui(
        const QString& final_name, const QString& source_desc
    );

    yodau::backend::stream_manager* stream_mgr { nullptr };
    settings_panel* settings { nullptr };
    board* main_zone { nullptr };
    grid_view* grid { nullptr };
    QString active_name;
};

#endif // YODAU_FRONTEND_HELPERS_CONTROLLER_HPP
