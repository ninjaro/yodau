#ifndef YODAU_FRONTEND_MAIN_WINDOW_TESTS_HPP
#define YODAU_FRONTEND_MAIN_WINDOW_TESTS_HPP

#include <QObject>

class main_window_tests : public QObject {
    Q_OBJECT

private slots:
    void active_edit_session_tracks_draft_templates_and_lines();
    void processing_feedback_state_tracks_log_details_and_motion_throttle();
    void stream_catalog_state_tracks_settings_and_local_sources();
    void stream_route_state_tracks_active_stream_and_add_validation();
    void stream_widget_bridge_applies_active_state_and_template_preview();
    void frontend_settings_normalize_algorithm_and_line_width();
    void format_frontend_log_entry_distinguishes_release_and_debug();
    void settings_panel_filters_logs_by_area_and_mode();
    void settings_panel_filters_logs_by_severity_stream_and_subsystem();
    void settings_panel_round_trips_structured_settings();
    void settings_panel_exposes_explicit_edit_panels();
    void settings_panel_emits_structured_stream_settings();
    void settings_panel_updates_algorithm_presets_by_selection();
    void settings_panel_exports_current_filtered_log_report();
    void stream_cell_tracks_stream_settings();
    void stream_cell_overlay_modes_render_different_event_regions();
    void stream_cell_line_profiles_render_different_wave_regions();
};

#endif // YODAU_FRONTEND_MAIN_WINDOW_TESTS_HPP
