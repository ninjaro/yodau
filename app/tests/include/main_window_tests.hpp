#ifndef YODAU_FRONTEND_MAIN_WINDOW_TESTS_HPP
#define YODAU_FRONTEND_MAIN_WINDOW_TESTS_HPP

#include <QObject>

class main_window_tests : public QObject {
    Q_OBJECT

private slots:
    void active_edit_actions_apply_lines_and_templates_against_backend();
    void active_edit_workflow_tracks_logs_and_follow_up_actions();
    void active_stream_workflow_tracks_selection_logs_and_follow_up();
    void active_editor_bridge_tracks_editor_projection_and_preview();
    void active_stream_state_tracks_selection_and_settings_application();
    void active_edit_controller_tracks_editor_sync_and_resets();
    void active_edit_session_tracks_draft_templates_and_lines();
    void processing_feedback_state_tracks_log_details_and_motion_throttle();
    void stream_catalog_state_tracks_settings_and_local_sources();
    void stream_catalog_workflow_tracks_seed_add_and_local_sources();
    void stream_route_state_tracks_active_stream_and_add_validation();
    void stream_widget_bridge_applies_active_state_and_template_preview();
    void stream_widget_bridge_syncs_active_editor_controls();
    void stream_widget_bridge_registers_and_routes_grid_visibility();
    void stream_inventory_panel_tracks_entries_and_visibility_signal();
    void stream_source_panel_tracks_modes_validation_and_requests();
    void active_stream_panel_tracks_selection_modes_and_settings();
    void active_editor_panel_tracks_active_tools_and_log();
    void log_toolbar_panel_filters_formats_and_emits_actions();
    void log_area_view_tracks_toolbar_refresh_and_fallback_append();
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
