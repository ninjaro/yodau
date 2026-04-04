#include "shell/stream_catalog_state.hpp"

#include <algorithm>

namespace stream_catalog_state_support {

QString trimmed_name(const QString& value) { return value.trimmed(); }

int normalized_manual_fps(const int fps, const int fallback) {
    return std::clamp(fps > 0 ? fps : fallback, 1, 120);
}

int normalized_manual_processing_pixels(const int pixels) {
    return std::clamp(
        pixels > 0 ? pixels : default_manual_processing_pixels(),
        16 * 16, 7680 * 4320
    );
}

} // namespace stream_catalog_state_support

stream_settings stream_catalog_state::default_stream_settings(
    const QString& stream_name
) {
    return normalized_stream_settings(stream_settings {
        .stream_name = normalized_stream_name(stream_name),
        .labels_enabled = true,
        .algorithm_id = default_app_algorithm_id(),
        .algorithm_preset = default_algorithm_preset_id(
            default_app_algorithm_id()
        ),
    });
}

stream_settings stream_catalog_state::normalized_stream_settings(
    stream_settings settings_value
) {
    settings_value.stream_name = normalized_stream_name(settings_value.stream_name);
    settings_value.algorithm_id
        = normalized_app_algorithm_id(settings_value.algorithm_id);
    settings_value.algorithm_preset = normalized_algorithm_preset_id(
        settings_value.algorithm_id, settings_value.algorithm_preset
    );
    settings_value.manual_display_fps
        = stream_catalog_state_support::normalized_manual_fps(
            settings_value.manual_display_fps, default_manual_display_fps()
        );
    settings_value.manual_core_fps
        = stream_catalog_state_support::normalized_manual_fps(
            settings_value.manual_core_fps, default_manual_core_fps()
        );
    settings_value.manual_processing_pixels
        = stream_catalog_state_support::normalized_manual_processing_pixels(
            settings_value.manual_processing_pixels
        );
    return settings_value;
}

QStringList stream_catalog_state::detected_local_sources(
    const std::vector<std::string>& core_names
) {
    QStringList locals;

    for (const std::string& name : core_names) {
        const QString qname = normalized_stream_name(QString::fromStdString(name));
        if (!qname.isEmpty() && qname.startsWith(QStringLiteral("video"))) {
            locals.push_back(qname);
        }
    }

    return locals;
}

void stream_catalog_state::ensure_stream(const QString& stream_name) {
    const QString normalized_name = normalized_stream_name(stream_name);
    if (normalized_name.isEmpty()
        || settings_by_stream_.contains(normalized_name)) {
        return;
    }

    settings_by_stream_.insert(
        normalized_name, default_stream_settings(normalized_name)
    );
}

void stream_catalog_state::set_stream_settings(stream_settings settings_value) {
    settings_value = normalized_stream_settings(std::move(settings_value));
    if (settings_value.stream_name.isEmpty()) {
        return;
    }

    settings_by_stream_[settings_value.stream_name] = std::move(settings_value);
}

stream_settings
stream_catalog_state::settings_for(const QString& stream_name) const {
    const QString normalized_name = normalized_stream_name(stream_name);
    if (normalized_name.isEmpty()) {
        return default_stream_settings(QString());
    }

    const auto it = settings_by_stream_.constFind(normalized_name);
    return it == settings_by_stream_.cend() ? default_stream_settings(normalized_name)
                                            : it.value();
}

QString stream_catalog_state::algorithm_id_for(const QString& stream_name) const {
    return settings_for(stream_name).algorithm_id;
}

QString stream_catalog_state::normalized_stream_name(const QString& stream_name) {
    return stream_catalog_state_support::trimmed_name(stream_name);
}
