#ifndef YODAU_APP_MAIN_WINDOW_TESTS_HPP
#define YODAU_APP_MAIN_WINDOW_TESTS_HPP

#include <QObject>

class main_window_tests : public QObject {
    Q_OBJECT

private slots:
    void active_actions_apply_lines_and_templates_against_core();
    void active_edit_tracks_logs_and_follow_up_actions();
    void active_stream_tracks_selection_logs_and_follow_up();
    void active_editor_bridge_tracks_editor_projection_and_preview();
    void active_stream_state_tracks_selection_and_settings_application();
    void active_edit_controller_tracks_editor_sync_and_resets();
    void active_edit_session_tracks_draft_templates_and_lines();
    void feedback_state_tracks_log_details_and_motion_throttle();
    void stream_catalog_state_tracks_settings_and_local_sources();
    void stream_catalog_tracks_seed_add_and_local_sources();
    void controller_adds_and_focuses_file_and_local_sources();
    void stream_route_tracks_active_stream_and_add_validation();
    void stream_bridge_applies_active_state_and_template_preview();
    void stream_widget_bridge_syncs_active_editor_controls();
    void stream_widget_bridge_registers_and_routes_grid_visibility();
    void stream_inventory_panel_tracks_entries_and_visibility_signal();
    void stream_source_panel_tracks_modes_validation_and_requests();
    void active_stream_panel_tracks_selection_modes_and_settings();
    void active_editor_panel_tracks_active_tools_and_log();
    void active_editor_exposes_line_edit_tab_and_signals();
    void active_editor_applies_stream_driven_line_edit_updates();
    void active_editor_panel_rotates_line_edit_preview();
    void active_edit_actions_detach_stream_line_preserves_template();
    void active_actions_detach_old_line_when_saving_variant();
    void log_toolbar_panel_filters_formats_and_emits_actions();
    void log_view_tracks_toolbar_refresh_and_fallback_append();
    void app_settings_normalize_algorithm_and_line_width();
    void format_app_log_entry_distinguishes_release_and_debug();
    void settings_panel_filters_logs_by_area_and_mode();
    void settings_filters_logs_by_severity_stream_and_subsystem();
    void settings_panel_round_trips_structured_settings();
    void settings_panel_exposes_explicit_edit_panels();
    void settings_panel_emits_structured_stream_settings();
    void settings_panel_emits_log_mode_changes();
    void settings_panel_updates_algorithm_presets_by_selection();
    void settings_panel_exports_current_filtered_log_report();
    void stream_widget_bridge_syncs_visible_log_mode();
    void stream_cell_tracks_stream_settings();
    void stream_cell_coalesces_frames_before_image_conversion();
    void stream_cell_negative_auto_masks_underlying_content();
    void stream_cell_emits_line_edit_interaction_signals();
    void stream_cell_status_badges_render_different_modes();
    void stream_cell_overlay_modes_render_different_event_regions();
    void stream_cell_line_profiles_render_different_wave_regions();
    void configuration_import_export_round_trips_through_controller();
    void main_window_exposes_profile_shell_and_shared_workflows();
    void window_state_store_round_trips_shared_layout();
    void window_state_store_rejects_incompatible_schema();
    void mobile_session_store_round_trips_and_rejects_unsafe_state();
    void app_log_bounds_history_and_updates_views_incrementally();
    void stream_cell_exposes_keyboard_creation_and_accessible_controls();
};

#endif // YODAU_APP_MAIN_WINDOW_TESTS_HPP
