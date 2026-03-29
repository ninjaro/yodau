#ifndef YODAU_FRONTEND_MAIN_WINDOW_TESTS_HPP
#define YODAU_FRONTEND_MAIN_WINDOW_TESTS_HPP

#include <QObject>

class main_window_tests : public QObject {
    Q_OBJECT

private slots:
    void format_frontend_log_entry_distinguishes_release_and_debug();
    void settings_panel_filters_logs_by_area_and_mode();
    void settings_panel_filters_logs_by_severity_stream_and_subsystem();
};

#endif // YODAU_FRONTEND_MAIN_WINDOW_TESTS_HPP
