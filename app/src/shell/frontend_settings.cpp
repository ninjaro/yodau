#include "shell/frontend_settings.hpp"

#include <algorithm>
#include <cmath>
#include <QRandomGenerator>

namespace frontend_settings_support {

QString normalized_key(QString text) {
    text = text.trimmed().toLower();
    text.replace(' ', '_');
    text.replace('-', '_');
    return text;
}

int normalized_manual_fps(const int fps, const int fallback) {
    return std::clamp(fps > 0 ? fps : fallback, 1, 120);
}

int normalized_manual_processing_pixels(const int pixels) {
    return std::clamp(
        pixels > 0 ? pixels : default_manual_processing_pixels(),
        16 * 16, 7680 * 4320
    );
}

QString compact_pixels_text(const int pixels) {
    if (pixels >= 1000 * 1000) {
        return QStringLiteral("%1M px").arg(
            QString::number(
                static_cast<double>(pixels) / 1000.0 / 1000.0, 'f', 2
            )
        );
    }

    if (pixels >= 1000) {
        return QStringLiteral("%1k px").arg(
            QString::number(static_cast<double>(pixels) / 1000.0, 'f', 0)
        );
    }

    return QStringLiteral("%1 px").arg(pixels);
}

} // namespace frontend_settings_support

QString default_frontend_algorithm_id() {
    return QStringLiteral("motion_baseline");
}

QStringList frontend_algorithm_ids() {
    return {
        QStringLiteral("motion_baseline"),
        QStringLiteral("hybrid_auto"),
        QStringLiteral("spot_grid"),
        QStringLiteral("contour_mask"),
    };
}

QString frontend_algorithm_display_name(const QString& algorithm_id) {
    const QString normalized
        = normalized_frontend_algorithm_id(algorithm_id);

    if (normalized == QStringLiteral("motion_baseline")) {
        return QStringLiteral("motion baseline");
    }
    if (normalized == QStringLiteral("spot_grid")) {
        return QStringLiteral("spot grid");
    }
    if (normalized == QStringLiteral("contour_mask")) {
        return QStringLiteral("contour mask");
    }
    if (normalized == QStringLiteral("hybrid_auto")) {
        return QStringLiteral("hybrid auto");
    }

    return QStringLiteral("motion baseline");
}

QString default_algorithm_preset_id(const QString& algorithm_id) {
    const QString normalized
        = normalized_frontend_algorithm_id(algorithm_id);

    if (normalized == QStringLiteral("spot_grid")) {
        return QStringLiteral("balanced");
    }
    if (normalized == QStringLiteral("contour_mask")) {
        return QStringLiteral("outline");
    }
    if (normalized == QStringLiteral("hybrid_auto")) {
        return QStringLiteral("adaptive");
    }

    return QStringLiteral("balanced");
}

QStringList algorithm_preset_ids(const QString& algorithm_id) {
    const QString normalized
        = normalized_frontend_algorithm_id(algorithm_id);

    if (normalized == QStringLiteral("spot_grid")) {
        return {
            QStringLiteral("coarse"),
            QStringLiteral("balanced"),
            QStringLiteral("dense"),
        };
    }

    if (normalized == QStringLiteral("hybrid_auto")) {
        return {
            QStringLiteral("load_guard"),
            QStringLiteral("adaptive"),
            QStringLiteral("tripwire_bias"),
        };
    }

    if (normalized == QStringLiteral("contour_mask")) {
        return {
            QStringLiteral("outline"),
            QStringLiteral("balanced"),
            QStringLiteral("mask_heavy"),
        };
    }

    return {
        QStringLiteral("simple"),
        QStringLiteral("balanced"),
        QStringLiteral("debug"),
    };
}

QString algorithm_preset_display_name(
    const QString& algorithm_id, const QString& preset_id
) {
    const QString normalized_algorithm
        = normalized_frontend_algorithm_id(algorithm_id);
    const QString normalized_preset
        = normalized_algorithm_preset_id(algorithm_id, preset_id);

    if (normalized_algorithm == QStringLiteral("spot_grid")) {
        if (normalized_preset == QStringLiteral("coarse")) {
            return QStringLiteral("coarse");
        }
        if (normalized_preset == QStringLiteral("dense")) {
            return QStringLiteral("dense");
        }
        return QStringLiteral("balanced");
    }

    if (normalized_algorithm == QStringLiteral("hybrid_auto")) {
        if (normalized_preset == QStringLiteral("load_guard")) {
            return QStringLiteral("load guard");
        }
        if (normalized_preset == QStringLiteral("tripwire_bias")) {
            return QStringLiteral("tripwire bias");
        }
        return QStringLiteral("adaptive");
    }

    if (normalized_algorithm == QStringLiteral("contour_mask")) {
        if (normalized_preset == QStringLiteral("outline")) {
            return QStringLiteral("outline");
        }
        if (normalized_preset == QStringLiteral("mask_heavy")) {
            return QStringLiteral("mask heavy");
        }
        return QStringLiteral("balanced");
    }

    if (normalized_preset == QStringLiteral("simple")) {
        return QStringLiteral("simple");
    }
    if (normalized_preset == QStringLiteral("debug")) {
        return QStringLiteral("debug");
    }

    return QStringLiteral("balanced");
}

QString normalized_algorithm_preset_id(
    const QString& algorithm_id, const QString& preset_id
) {
    const QString normalized_algorithm
        = normalized_frontend_algorithm_id(algorithm_id);
    const QString normalized_preset
        = frontend_settings_support::normalized_key(preset_id);

    const QStringList valid_presets = algorithm_preset_ids(normalized_algorithm);
    if (valid_presets.contains(normalized_preset)) {
        return normalized_preset;
    }

    if (normalized_algorithm == QStringLiteral("spot_grid")) {
        if (normalized_preset == QStringLiteral("simple")) {
            return QStringLiteral("coarse");
        }
        if (normalized_preset == QStringLiteral("debug")) {
            return QStringLiteral("dense");
        }
    }

    if (normalized_algorithm == QStringLiteral("hybrid_auto")) {
        if (normalized_preset == QStringLiteral("simple")) {
            return QStringLiteral("load_guard");
        }
        if (normalized_preset == QStringLiteral("debug")) {
            return QStringLiteral("tripwire_bias");
        }
    }

    if (normalized_algorithm == QStringLiteral("contour_mask")) {
        if (normalized_preset == QStringLiteral("simple")) {
            return QStringLiteral("outline");
        }
        if (normalized_preset == QStringLiteral("debug")) {
            return QStringLiteral("mask_heavy");
        }
    }

    return default_algorithm_preset_id(normalized_algorithm);
}

QString algorithm_summary_text(
    const QString& algorithm_id, const QString& preset_id,
    const bool overlay_enabled
) {
    const QString algorithm_name = frontend_algorithm_display_name(algorithm_id);
    const QString preset_name
        = algorithm_preset_display_name(algorithm_id, preset_id);
    const QString overlay_text = overlay_enabled
        ? QStringLiteral("diagnostic overlay enabled")
        : QStringLiteral("diagnostic overlay hidden");

    if (normalized_frontend_algorithm_id(algorithm_id)
        == QStringLiteral("spot_grid")) {
        return QStringLiteral(
            "%1 uses point-style motion regions with a %2 preset; %3."
        )
            .arg(algorithm_name)
            .arg(preset_name)
            .arg(overlay_text);
    }

    if (normalized_frontend_algorithm_id(algorithm_id)
        == QStringLiteral("hybrid_auto")) {
        return QStringLiteral(
            "%1 adapts between load guard, tripwire bias, and contour-heavy "
            "modes with a %2 preset; %3."
        )
            .arg(algorithm_name)
            .arg(preset_name)
            .arg(overlay_text);
    }

    if (normalized_frontend_algorithm_id(algorithm_id)
        == QStringLiteral("contour_mask")) {
        return QStringLiteral(
            "%1 emphasizes contours and masks with a %2 preset; %3."
        )
            .arg(algorithm_name)
            .arg(preset_name)
            .arg(overlay_text);
    }

    return QStringLiteral(
        "%1 keeps the baseline motion path with a %2 preset; %3."
    )
        .arg(algorithm_name)
        .arg(preset_name)
        .arg(overlay_text);
}

QString algorithm_badge_text(
    const QString& algorithm_id, const QString& preset_id,
    const bool overlay_enabled
) {
    stream_settings settings_value;
    settings_value.algorithm_id = algorithm_id;
    settings_value.algorithm_preset = preset_id;
    settings_value.algorithm_overlay_enabled = overlay_enabled;

    const QString normalized_algorithm = normalized_frontend_algorithm_id(
        settings_value.algorithm_id
    );
    const QString profile_id = inferred_operator_profile_id(settings_value);
    const QString short_algorithm
        = normalized_algorithm == QStringLiteral("spot_grid")
        ? QStringLiteral("SG")
        : (normalized_algorithm == QStringLiteral("hybrid_auto")
               ? QStringLiteral("HA")
        : (normalized_algorithm == QStringLiteral("contour_mask")
               ? QStringLiteral("CM")
               : QStringLiteral("MB")));

    return QStringLiteral("%1 %2")
        .arg(short_algorithm)
        .arg(
            profile_id == QStringLiteral("custom")
                ? QStringLiteral("custom")
                : operator_profile_display_name(profile_id)
        );
}

QColor algorithm_badge_color(const QString& algorithm_id) {
    const QString normalized
        = normalized_frontend_algorithm_id(algorithm_id);

    if (normalized == QStringLiteral("spot_grid")) {
        return QColor(QStringLiteral("#2a9d8f"));
    }
    if (normalized == QStringLiteral("hybrid_auto")) {
        return QColor(QStringLiteral("#9c6644"));
    }
    if (normalized == QStringLiteral("contour_mask")) {
        return QColor(QStringLiteral("#e76f51"));
    }

    return QColor(QStringLiteral("#577590"));
}

QString normalized_frontend_algorithm_id(const QString& algorithm_id) {
    const QString normalized
        = frontend_settings_support::normalized_key(algorithm_id);

    if (normalized.isEmpty() || normalized == QStringLiteral("default")
        || normalized == QStringLiteral("baseline")
        || normalized == QStringLiteral("motion")
        || normalized == QStringLiteral("motion_baseline")) {
        return QStringLiteral("motion_baseline");
    }

    if (normalized == QStringLiteral("spot")
        || normalized == QStringLiteral("spots")
        || normalized == QStringLiteral("spot_grid")) {
        return QStringLiteral("spot_grid");
    }

    if (normalized == QStringLiteral("hybrid")
        || normalized == QStringLiteral("auto")
        || normalized == QStringLiteral("adaptive")
        || normalized == QStringLiteral("hybrid_auto")) {
        return QStringLiteral("hybrid_auto");
    }

    if (normalized == QStringLiteral("contour")
        || normalized == QStringLiteral("contours")
        || normalized == QStringLiteral("mask")
        || normalized == QStringLiteral("contour_mask")) {
        return QStringLiteral("contour_mask");
    }

    return QStringLiteral("motion_baseline");
}

QString default_operator_profile_id() { return QStringLiteral("balanced"); }

QStringList operator_profile_ids(const bool include_custom) {
    QStringList ids {
        QStringLiteral("simple"),
        QStringLiteral("balanced"),
        QStringLiteral("debug_heavy"),
    };

    if (include_custom) {
        ids.push_back(QStringLiteral("custom"));
    }

    return ids;
}

QString operator_profile_display_name(const QString& profile_id) {
    const QString normalized = normalized_operator_profile_id(profile_id);

    if (normalized == QStringLiteral("simple")) {
        return QStringLiteral("simple");
    }
    if (normalized == QStringLiteral("debug_heavy")) {
        return QStringLiteral("debug-heavy");
    }
    if (normalized == QStringLiteral("custom")) {
        return QStringLiteral("custom");
    }

    return QStringLiteral("balanced");
}

QString normalized_operator_profile_id(const QString& profile_id) {
    const QString normalized
        = frontend_settings_support::normalized_key(profile_id);

    if (normalized.isEmpty() || normalized == QStringLiteral("default")
        || normalized == QStringLiteral("balanced")) {
        return QStringLiteral("balanced");
    }

    if (normalized == QStringLiteral("simple")
        || normalized == QStringLiteral("basic")) {
        return QStringLiteral("simple");
    }

    if (normalized == QStringLiteral("debug")
        || normalized == QStringLiteral("debug_heavy")
        || normalized == QStringLiteral("debugheavy")) {
        return QStringLiteral("debug_heavy");
    }

    if (normalized == QStringLiteral("custom")
        || normalized == QStringLiteral("manual")
        || normalized == QStringLiteral("mixed")) {
        return QStringLiteral("custom");
    }

    return default_operator_profile_id();
}

stream_settings apply_operator_profile(
    stream_settings settings_value, const QString& profile_id
) {
    const QString normalized_profile
        = normalized_operator_profile_id(profile_id);

    settings_value.algorithm_id = normalized_frontend_algorithm_id(
        settings_value.algorithm_id
    );

    if (normalized_profile == QStringLiteral("custom")) {
        settings_value.algorithm_preset = normalized_algorithm_preset_id(
            settings_value.algorithm_id, settings_value.algorithm_preset
        );
        return settings_value;
    }

    const QString preset_alias = normalized_profile == QStringLiteral("simple")
        ? QStringLiteral("simple")
        : (normalized_profile == QStringLiteral("debug_heavy")
               ? QStringLiteral("debug")
               : QStringLiteral("balanced"));

    settings_value.algorithm_preset = normalized_algorithm_preset_id(
        settings_value.algorithm_id, preset_alias
    );
    settings_value.algorithm_overlay_enabled
        = normalized_profile == QStringLiteral("debug_heavy");
    return settings_value;
}

QString inferred_operator_profile_id(const stream_settings& settings_value) {
    stream_settings normalized_settings = settings_value;
    normalized_settings.algorithm_id = normalized_frontend_algorithm_id(
        normalized_settings.algorithm_id
    );
    normalized_settings.algorithm_preset = normalized_algorithm_preset_id(
        normalized_settings.algorithm_id, normalized_settings.algorithm_preset
    );

    for (const QString& candidate : operator_profile_ids()) {
        const stream_settings candidate_settings
            = apply_operator_profile(normalized_settings, candidate);
        if (candidate_settings.algorithm_preset
                == normalized_settings.algorithm_preset
            && candidate_settings.algorithm_overlay_enabled
                == normalized_settings.algorithm_overlay_enabled) {
            return candidate;
        }
    }

    return QStringLiteral("custom");
}

QString operator_profile_summary_text(const stream_settings& settings_value) {
    const QString normalized_algorithm = normalized_frontend_algorithm_id(
        settings_value.algorithm_id
    );
    const QString profile_id
        = inferred_operator_profile_id(settings_value);

    if (profile_id == QStringLiteral("custom")) {
        return QStringLiteral(
            "custom keeps a manual preset and overlay mix for %1."
        )
            .arg(frontend_algorithm_display_name(normalized_algorithm));
    }

    stream_settings applied_input;
    applied_input.algorithm_id = normalized_algorithm;
    const stream_settings applied_settings
        = apply_operator_profile(applied_input, profile_id);
    const QString preset_name = algorithm_preset_display_name(
        normalized_algorithm, applied_settings.algorithm_preset
    );
    const QString overlay_text = applied_settings.algorithm_overlay_enabled
        ? QStringLiteral("diagnostic overlay on")
        : QStringLiteral("diagnostic overlay off");

    return QStringLiteral("%1 maps %2 to %3 with %4.")
        .arg(operator_profile_display_name(profile_id))
        .arg(frontend_algorithm_display_name(normalized_algorithm))
        .arg(preset_name)
        .arg(overlay_text);
}

QString default_line_width_text() { return QStringLiteral("medium"); }

QString default_line_color_mode_id() {
    return QStringLiteral("auto_palette");
}

QStringList line_color_mode_ids() {
    return {
        QStringLiteral("auto_palette"),
        QStringLiteral("negative_auto"),
        QStringLiteral("manual"),
    };
}

QString line_color_mode_display_name(const QString& mode_id) {
    const QString normalized = normalized_line_color_mode_id(mode_id);
    if (normalized == QStringLiteral("manual")) {
        return QStringLiteral("manual color");
    }
    if (normalized == QStringLiteral("negative_auto")) {
        return QStringLiteral("negative auto");
    }
    return QStringLiteral("auto palette");
}

QString normalized_line_color_mode_id(const QString& mode_id) {
    const QString normalized
        = frontend_settings_support::normalized_key(mode_id);

    if (normalized == QStringLiteral("manual")
        || normalized == QStringLiteral("fixed")
        || normalized == QStringLiteral("random")) {
        return QStringLiteral("manual");
    }
    if (normalized == QStringLiteral("negative_auto")
        || normalized == QStringLiteral("negative")
        || normalized == QStringLiteral("invert")
        || normalized == QStringLiteral("inverse")) {
        return QStringLiteral("negative_auto");
    }

    return QStringLiteral("auto_palette");
}

QColor random_manual_line_color() {
    const int hue = QRandomGenerator::global()->bounded(360);
    const int saturation = 165 + QRandomGenerator::global()->bounded(60);
    const int value = 210 + QRandomGenerator::global()->bounded(36);
    return QColor::fromHsv(hue, std::clamp(saturation, 0, 255), std::clamp(value, 0, 255));
}

QColor auto_palette_line_color(const int line_index, const int line_count) {
    const int clamped_count = std::max(1, line_count);
    const int clamped_index = std::clamp(line_index, 0, clamped_count - 1);
    const double ratio
        = static_cast<double>(clamped_index) / static_cast<double>(clamped_count);
    const int hue = static_cast<int>(std::lround(std::fmod(22.0 + ratio * 330.0, 360.0)));
    const int saturation = clamped_count <= 2 ? 150 : 178;
    const int value = clamped_count >= 7 ? 228 : 238;
    return QColor::fromHsv(hue, saturation, value);
}

QColor softened_negative_line_color(const QColor& sampled_color) {
    const QColor base = sampled_color.isValid() ? sampled_color : QColor(QStringLiteral("#456b88"));
    QColor inverted(
        255 - base.red(), 255 - base.green(), 255 - base.blue()
    );
    QColor softened(
        static_cast<int>(std::lround(inverted.red() * 0.72 + 255.0 * 0.28)),
        static_cast<int>(std::lround(inverted.green() * 0.72 + 255.0 * 0.28)),
        static_cast<int>(std::lround(inverted.blue() * 0.72 + 255.0 * 0.28))
    );
    softened.setAlpha(235);
    return softened;
}

int default_manual_display_fps() { return 24; }

int default_manual_backend_fps() { return 12; }

int default_manual_processing_pixels() { return 1280 * 720; }

QString default_line_parameter_mode_id() { return QStringLiteral("direct"); }

QStringList line_parameter_mode_ids() {
    return {
        QStringLiteral("direct"),
        QStringLiteral("instrument"),
    };
}

QString line_parameter_mode_display_name(const QString& mode_id) {
    const QString normalized = normalized_line_parameter_mode_id(mode_id);
    if (normalized == QStringLiteral("instrument")) {
        return QStringLiteral("string vocabulary");
    }

    return QStringLiteral("direct terms");
}

QString normalized_line_parameter_mode_id(const QString& mode_id) {
    const QString normalized
        = frontend_settings_support::normalized_key(mode_id);

    if (normalized == QStringLiteral("instrument")
        || normalized == QStringLiteral("string")
        || normalized == QStringLiteral("strings")
        || normalized == QStringLiteral("musical")
        || normalized == QStringLiteral("music")) {
        return QStringLiteral("instrument");
    }

    return QStringLiteral("direct");
}

QString line_width_label_text(const QString& mode_id) {
    if (normalized_line_parameter_mode_id(mode_id)
        == QStringLiteral("instrument")) {
        return QStringLiteral("string gauge");
    }

    return QStringLiteral("line thickness");
}

QString line_length_label_text(const QString& mode_id) {
    if (normalized_line_parameter_mode_id(mode_id)
        == QStringLiteral("instrument")) {
        return QStringLiteral("resonance span");
    }

    return QStringLiteral("effective span");
}

QString line_response_label_text(const QString& mode_id) {
    if (normalized_line_parameter_mode_id(mode_id)
        == QStringLiteral("instrument")) {
        return QStringLiteral("tension / damping");
    }

    return QStringLiteral("response / damping");
}

QString line_parameter_mode_hint_text(const QString& mode_id) {
    if (normalized_line_parameter_mode_id(mode_id)
        == QStringLiteral("instrument")) {
        return QStringLiteral(
            "String vocabulary is a naming layer for now; note, chord, and "
            "pitch physics are still future work."
        );
    }

    return QStringLiteral(
        "Effective span is not the literal line length; it shapes how far the "
        "interaction travels."
    );
}

QStringList suggested_line_width_texts() {
    return {
        QStringLiteral("thin"),
        QStringLiteral("medium"),
        QStringLiteral("thick"),
        QStringLiteral("string_light"),
        QStringLiteral("string_heavy"),
    };
}

QString normalized_line_width_text(const QString& width_text) {
    const QString normalized
        = frontend_settings_support::normalized_key(width_text);

    if (normalized.isEmpty() || normalized == QStringLiteral("default")
        || normalized == QStringLiteral("normal")
        || normalized == QStringLiteral("medium")) {
        return QStringLiteral("medium");
    }

    if (normalized == QStringLiteral("thin")
        || normalized == QStringLiteral("narrow")) {
        return QStringLiteral("thin");
    }

    if (normalized == QStringLiteral("thick")
        || normalized == QStringLiteral("wide")) {
        return QStringLiteral("thick");
    }

    if (normalized == QStringLiteral("string_light")
        || normalized == QStringLiteral("light")) {
        return QStringLiteral("string_light");
    }

    if (normalized == QStringLiteral("string_heavy")
        || normalized == QStringLiteral("heavy")) {
        return QStringLiteral("string_heavy");
    }

    bool ok = false;
    const double numeric_width = normalized.toDouble(&ok);
    if (ok && numeric_width > 0.0) {
        return QString::number(numeric_width, 'f', 1);
    }

    return QStringLiteral("medium");
}

qreal line_width_visual_value(const QString& width_text) {
    const QString normalized = normalized_line_width_text(width_text);

    if (normalized == QStringLiteral("thin")) {
        return 2.0;
    }
    if (normalized == QStringLiteral("medium")) {
        return 3.0;
    }
    if (normalized == QStringLiteral("thick")) {
        return 5.0;
    }
    if (normalized == QStringLiteral("string_light")) {
        return 3.5;
    }
    if (normalized == QStringLiteral("string_heavy")) {
        return 6.5;
    }

    bool ok = false;
    const double numeric_width = normalized.toDouble(&ok);
    if (ok && numeric_width > 0.0) {
        return std::clamp(numeric_width, 1.0, 12.0);
    }

    return 3.0;
}

QString default_line_length_text() { return QStringLiteral("medium"); }

QStringList suggested_line_length_texts() {
    return {
        QStringLiteral("short"),
        QStringLiteral("medium"),
        QStringLiteral("long"),
    };
}

QString normalized_line_length_text(const QString& length_text) {
    const QString normalized
        = frontend_settings_support::normalized_key(length_text);

    if (normalized.isEmpty() || normalized == QStringLiteral("default")
        || normalized == QStringLiteral("normal")
        || normalized == QStringLiteral("medium")
        || normalized == QStringLiteral("balanced")) {
        return QStringLiteral("medium");
    }

    if (normalized == QStringLiteral("short")
        || normalized == QStringLiteral("compact")
        || normalized == QStringLiteral("tight")) {
        return QStringLiteral("short");
    }

    if (normalized == QStringLiteral("long")
        || normalized == QStringLiteral("extended")
        || normalized == QStringLiteral("drone")) {
        return QStringLiteral("long");
    }

    bool ok = false;
    const double numeric_length = normalized.toDouble(&ok);
    if (ok && numeric_length > 0.0) {
        return QString::number(numeric_length, 'f', 1);
    }

    return QStringLiteral("medium");
}

QString default_line_response_text() { return QStringLiteral("balanced"); }

QStringList suggested_line_response_texts() {
    return {
        QStringLiteral("dry"),
        QStringLiteral("balanced"),
        QStringLiteral("resonant"),
    };
}

QString normalized_line_response_text(const QString& response_text) {
    const QString normalized
        = frontend_settings_support::normalized_key(response_text);

    if (normalized.isEmpty() || normalized == QStringLiteral("default")
        || normalized == QStringLiteral("normal")
        || normalized == QStringLiteral("balanced")) {
        return QStringLiteral("balanced");
    }

    if (normalized == QStringLiteral("dry")
        || normalized == QStringLiteral("muted")
        || normalized == QStringLiteral("tight")) {
        return QStringLiteral("dry");
    }

    if (normalized == QStringLiteral("resonant")
        || normalized == QStringLiteral("ringing")
        || normalized == QStringLiteral("sustain")
        || normalized == QStringLiteral("sustained")) {
        return QStringLiteral("resonant");
    }

    bool ok = false;
    const double numeric_response = normalized.toDouble(&ok);
    if (ok && numeric_response > 0.0) {
        return QString::number(numeric_response, 'f', 1);
    }

    return QStringLiteral("balanced");
}

QString line_profile_summary_text(
    const QString& width_text, const QString& length_text,
    const QString& response_text, const QString& parameter_mode_id
) {
    const QString normalized_mode
        = normalized_line_parameter_mode_id(parameter_mode_id);

    if (normalized_mode == QStringLiteral("instrument")) {
        return QStringLiteral("gauge=%1 span=%2 tension=%3")
            .arg(normalized_line_width_text(width_text))
            .arg(normalized_line_length_text(length_text))
            .arg(normalized_line_response_text(response_text));
    }

    return QStringLiteral("width=%1 length=%2 response=%3")
        .arg(normalized_line_width_text(width_text))
        .arg(normalized_line_length_text(length_text))
        .arg(normalized_line_response_text(response_text));
}

QString stream_processing_policy_summary_text(
    const stream_settings& settings_value
) {
    if (!settings_value.manual_processing_policy_enabled) {
        return QStringLiteral(
            "auto calibration picks display fps, backend fps, and processing "
            "pixels for this stream."
        );
    }

    return QStringLiteral(
        "manual caps display to %1 fps, backend processing to %2 fps, and "
        "processing input to %3."
    )
        .arg(
            frontend_settings_support::normalized_manual_fps(
                settings_value.manual_display_fps, default_manual_display_fps()
            )
        )
        .arg(
            frontend_settings_support::normalized_manual_fps(
                settings_value.manual_backend_fps, default_manual_backend_fps()
            )
        )
        .arg(
            frontend_settings_support::compact_pixels_text(
                frontend_settings_support::normalized_manual_processing_pixels(
                    settings_value.manual_processing_pixels
                )
            )
        );
}

QString stream_runtime_metrics_text(const stream_runtime_metrics& metrics) {
    const QString input_fps = metrics.input_fps > 0.0
        ? QString::number(metrics.input_fps, 'f', 1)
        : QStringLiteral("--");
    const QString backend_fps = metrics.backend_fps > 0.0
        ? QString::number(metrics.backend_fps, 'f', 1)
        : QStringLiteral("--");
    const QString mode = metrics.manual_policy_active ? QStringLiteral("manual")
                                                      : QStringLiteral("auto");
    const QString display_target = metrics.effective_display_fps > 0
        ? QString::number(metrics.effective_display_fps)
        : QStringLiteral("--");
    const QString backend_target = metrics.effective_backend_fps > 0
        ? QString::number(metrics.effective_backend_fps)
        : QStringLiteral("--");

    QString processed_size = QStringLiteral("--");
    if (metrics.processed_width > 0 && metrics.processed_height > 0) {
        processed_size = QStringLiteral("%1x%2")
            .arg(metrics.processed_width)
            .arg(metrics.processed_height);
    } else if (metrics.effective_processing_pixels > 0) {
        processed_size = frontend_settings_support::compact_pixels_text(
            metrics.effective_processing_pixels
        );
    }

    return QStringLiteral(
        "video %1 fps | backend %2 fps\n%3 %4/%5 fps | %6"
    )
        .arg(input_fps)
        .arg(backend_fps)
        .arg(mode)
        .arg(display_target)
        .arg(backend_target)
        .arg(processed_size);
}
