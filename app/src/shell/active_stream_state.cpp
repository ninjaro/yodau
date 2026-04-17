#include "shell/active_stream_state.hpp"

#include "shell/active_edit_session.hpp"
#include "shell/stream_catalog_state.hpp"
#include "shell/stream_route_state.hpp"
#include "shell/stream_widget_bridge.hpp"

#include <utility>

active_stream_state::active_stream_state(
    stream_catalog_state& catalog_state, stream_route_state& route_state,
    stream_widget_bridge& widget_bridge, active_edit_session& edit_session
)
    : catalog_state_(catalog_state)
    , route_state_(route_state)
    , widget_bridge_(widget_bridge)
    , edit_session_(edit_session) {}

active_stream_state::selection_result active_stream_state::set_active_stream(
    const QString& name
) const {
    selection_result result;

    const QString previous_active_name = route_state_.active_stream_name();
    route_state_.set_active_stream(name);

    result.active_name = route_state_.active_stream_name();
    result.changed = result.active_name != previous_active_name;
    result.settings = catalog_state_.settings_for(result.active_name);

    widget_bridge_.apply_active_stream(
        result.active_name, result.settings, edit_session_
    );
    widget_bridge_.sync_active_persistent(result.active_name, edit_session_);

    return result;
}

active_stream_state::settings_result active_stream_state::apply_stream_settings(
    stream_settings settings_value
) const {
    settings_result result;

    settings_value = stream_catalog_state::normalized_stream_settings(
        std::move(settings_value)
    );
    result.previous_active_name = route_state_.active_stream_name();
    result.active_name = result.previous_active_name;

    if (settings_value.stream_name.isEmpty()) {
        return result;
    }

    result.previous_settings = catalog_state_.settings_for(
        settings_value.stream_name
    );
    catalog_state_.set_stream_settings(settings_value);
    result.settings = catalog_state_.settings_for(settings_value.stream_name);
    result.labels_changed
        = result.previous_settings.labels_enabled != result.settings.labels_enabled;
    result.standard_labels_changed
        = result.previous_settings.standard_labels_enabled
        != result.settings.standard_labels_enabled;
    result.algorithm_changed
        = result.previous_settings.algorithm_id != result.settings.algorithm_id;
    result.algorithm_changed = result.algorithm_changed
        || result.previous_settings.algorithm_preset
            != result.settings.algorithm_preset
        || result.previous_settings.movement_display_mode
            != result.settings.movement_display_mode;
    result.processing_policy_changed
        = result.previous_settings.manual_processing_policy_enabled
            != result.settings.manual_processing_policy_enabled
        || result.previous_settings.manual_display_fps
            != result.settings.manual_display_fps
        || result.previous_settings.manual_core_fps
            != result.settings.manual_core_fps
        || result.previous_settings.manual_processing_pixels
            != result.settings.manual_processing_pixels;

    widget_bridge_.sync_stream_visual_settings(
        result.settings.stream_name, result.settings, route_state_
    );

    if (result.settings.stream_name == result.active_name) {
        widget_bridge_.sync_active_selection(
            result.active_name, result.settings
        );
    }

    result.outcome_value = settings_result::outcome::updated;
    return result;
}
