#include "shell/stream_widget_bridge.hpp"

#include "shell/active_edit_session.hpp"
#include "shell/stream_route_state.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/settings_panel.hpp"
#include "widgets/stream_board.hpp"
#include "widgets/stream_cell.hpp"

#include <QUrl>

namespace stream_widget_bridge_support {

void apply_grid_stream_binding(
    stream_cell* cell, const stream_widget_bridge::grid_stream_binding& binding
) {
    if (cell == nullptr) {
        return;
    }

    cell->set_loop(binding.loop);
    if (binding.path.isEmpty()) {
        return;
    }

    if (binding.type == QStringLiteral("local")) {
        cell->set_camera_id(binding.path.toUtf8());
        return;
    }

    if (binding.type == QStringLiteral("file")) {
        cell->set_source(QUrl::fromLocalFile(binding.path));
        return;
    }

    cell->set_source(QUrl(binding.path));
}

} // namespace stream_widget_bridge_support

stream_widget_bridge::stream_widget_bridge(
    stream_board* main_zone, settings_panel* settings
)
    : editor_bridge_(main_zone, settings)
    , settings_(settings)
    , main_zone_(main_zone)
    , grid_(main_zone != nullptr ? main_zone->grid_mode() : nullptr) {}

grid_view* stream_widget_bridge::grid() const { return grid_; }

stream_cell* stream_widget_bridge::active_cell() const {
    return main_zone_ != nullptr ? main_zone_->active_cell() : nullptr;
}

stream_cell* stream_widget_bridge::tile_for_stream_name(
    const QString& name, const stream_route_state& route_state
) const {
    if (main_zone_ != nullptr && route_state.is_active_stream(name)) {
        if (auto* cell = main_zone_->active_cell()) {
            return cell;
        }
    }

    if (grid_ != nullptr) {
        if (auto* tile = grid_->peek_stream_cell(name)) {
            return tile;
        }
    }

    return nullptr;
}

void stream_widget_bridge::initialize_editor_state(
    const active_edit_session& edit_session
) const {
    editor_bridge_.initialize_editor_state(edit_session);
}

void stream_widget_bridge::sync_active_line_editor(
    const active_edit_session& edit_session, const bool reset_form
) const {
    editor_bridge_.sync_active_line_editor(edit_session, reset_form);
}

void stream_widget_bridge::sync_active_template_editor(
    const active_edit_session& edit_session, const bool reset_form
) const {
    editor_bridge_.sync_active_template_editor(edit_session, reset_form);
}

void stream_widget_bridge::add_template_candidate(const QString& name) const {
    editor_bridge_.add_template_candidate(name);
}

void stream_widget_bridge::register_stream_entry(
    const QString& final_name, const QString& source_desc
) const {
    if (settings_ == nullptr) {
        return;
    }

    settings_->add_existing_name(final_name);
    settings_->add_stream_entry(final_name, source_desc);
    settings_->clear_add_inputs();
    editor_bridge_.sync_active_candidates();
}

stream_cell* stream_widget_bridge::show_stream_in_grid(
    const QString& name, const stream_settings& settings_value,
    const active_edit_session& edit_session, const grid_stream_binding& binding
) const {
    if (grid_ == nullptr || name.isEmpty()) {
        return nullptr;
    }

    grid_->add_stream(name);
    auto* tile = grid_->peek_stream_cell(name);
    if (tile == nullptr) {
        return nullptr;
    }

    tile->set_persistent_lines(edit_session.stream_lines(name));
    tile->set_stream_settings(settings_value);
    tile->set_labels_enabled(settings_value.labels_enabled);
    tile->set_log_mode(
        settings_ != nullptr ? settings_->log_mode()
                             : frontend_log_mode::release
    );
    stream_widget_bridge_support::apply_grid_stream_binding(tile, binding);
    editor_bridge_.sync_active_candidates();
    return tile;
}

void stream_widget_bridge::hide_stream_from_grid(
    const QString& name, const bool clear_active
) const {
    if (grid_ != nullptr) {
        grid_->remove_stream(name);
    }

    if (clear_active && main_zone_ != nullptr) {
        if (auto* cell = main_zone_->take_active_cell()) {
            cell->deleteLater();
        }
        editor_bridge_.sync_active_selection(QString(), stream_settings {});
    }

    editor_bridge_.sync_active_candidates();
}

void stream_widget_bridge::sync_active_candidates() const {
    editor_bridge_.sync_active_candidates();
}

void stream_widget_bridge::sync_visible_log_mode(
    const frontend_log_mode mode
) const {
    if (grid_ != nullptr) {
        for (const QString& name : grid_->stream_names()) {
            if (auto* tile = grid_->peek_stream_cell(name)) {
                tile->set_log_mode(mode);
            }
        }
    }

    if (main_zone_ != nullptr) {
        if (auto* cell = main_zone_->active_cell()) {
            cell->set_log_mode(mode);
        }
    }
}

void stream_widget_bridge::sync_active_selection(
    const QString& active_name, const stream_settings& settings_value
) const {
    editor_bridge_.sync_active_selection(active_name, settings_value);
}

void stream_widget_bridge::sync_stream_visual_settings(
    const QString& name, const stream_settings& settings_value,
    const stream_route_state& route_state
) const {
    if (name.isEmpty()) {
        return;
    }

    if (auto* cell = tile_for_stream_name(name, route_state)) {
        cell->set_stream_settings(settings_value);
        cell->set_labels_enabled(settings_value.labels_enabled);
    }
}

void stream_widget_bridge::apply_active_stream(
    const QString& active_name, const stream_settings& settings_value,
    const active_edit_session& edit_session
) const {
    editor_bridge_.apply_active_stream(active_name, settings_value, edit_session);
}

void stream_widget_bridge::sync_active_persistent(
    const QString& active_name, const active_edit_session& edit_session
) const {
    editor_bridge_.sync_active_persistent(active_name, edit_session);
}

void stream_widget_bridge::apply_template_preview(
    const QString& template_name, const active_edit_session& edit_session
) const {
    editor_bridge_.apply_template_preview(template_name, edit_session);
}
