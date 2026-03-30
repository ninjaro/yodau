#include "shell/frontend_settings.hpp"

#include <algorithm>

namespace frontend_settings_support {

QString normalized_key(QString text) {
    text = text.trimmed().toLower();
    text.replace(' ', '_');
    text.replace('-', '_');
    return text;
}

} // namespace frontend_settings_support

QString default_frontend_algorithm_id() {
    return QStringLiteral("motion_baseline");
}

QStringList frontend_algorithm_ids() {
    return {
        QStringLiteral("motion_baseline"),
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
    const QString normalized_algorithm
        = normalized_frontend_algorithm_id(algorithm_id);
    const QString normalized_preset
        = normalized_algorithm_preset_id(algorithm_id, preset_id);

    QString badge;
    if (normalized_algorithm == QStringLiteral("spot_grid")) {
        badge = QStringLiteral("SG");
    } else if (normalized_algorithm == QStringLiteral("contour_mask")) {
        badge = QStringLiteral("CM");
    } else {
        badge = QStringLiteral("MB");
    }

    badge += QStringLiteral(":%1").arg(normalized_preset);
    if (overlay_enabled) {
        badge += QStringLiteral(" overlay");
    }

    return badge;
}

QColor algorithm_badge_color(const QString& algorithm_id) {
    const QString normalized
        = normalized_frontend_algorithm_id(algorithm_id);

    if (normalized == QStringLiteral("spot_grid")) {
        return QColor(QStringLiteral("#2a9d8f"));
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

    if (normalized == QStringLiteral("contour")
        || normalized == QStringLiteral("contours")
        || normalized == QStringLiteral("mask")
        || normalized == QStringLiteral("contour_mask")) {
        return QStringLiteral("contour_mask");
    }

    return QStringLiteral("motion_baseline");
}

QString default_line_width_text() { return QStringLiteral("medium"); }

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
    const QString& response_text
) {
    return QStringLiteral("width=%1 length=%2 response=%3")
        .arg(normalized_line_width_text(width_text))
        .arg(normalized_line_length_text(length_text))
        .arg(normalized_line_response_text(response_text));
}
