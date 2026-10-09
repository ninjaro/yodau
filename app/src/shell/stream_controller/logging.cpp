#include "shell/stream_controller.hpp"
#include "widgets/settings_panel.hpp"
#include "widgets/stream_cell.hpp"

#include <QColor>
#include <QDateTime>
#include <algorithm>
#include <utility>

void stream_controller::append_log_entry(app_log_entry entry) const {
    if (log_buffer != nullptr) {
        log_buffer->append(std::move(entry));
        return;
    }

    if (settings != nullptr) {
        settings->append_log(std::move(entry));
    }
}

void stream_controller::append_log_entries(
    const QVector<app_log_entry>& entries
) const {
    for (const app_log_entry& entry : entries) {
        append_log_entry(entry);
    }
}

void stream_controller::append_log(
    const app_log_area area, const app_log_severity severity,
    const QString& subsystem, const QString& message,
    const QString& stream_name, const QString& detail,
    const QString& algorithm_id, const QString& line_name,
    const QString& event_type, const QColor& line_color
) const {
    const QString resolved_algorithm_id = stream_name.isEmpty()
        ? QString()
        : (algorithm_id.isEmpty() ? algorithm_id_for_stream(stream_name)
                                  : normalized_app_algorithm_id(algorithm_id));

    app_log_entry entry {
        .timestamp = QDateTime(),
        .area = area,
        .severity = severity,
        .subsystem = subsystem,
        .stream_name = stream_name,
        .line_name = line_name,
        .algorithm_id = resolved_algorithm_id,
        .event_type = event_type,
        .message = message,
        .detail = detail,
        .line_color = line_color,
    };

    append_log_entry(std::move(entry));
}

QColor stream_controller::resolved_log_line_color(
    const QString& stream_name, const QString& line_name
) const {
    if (stream_name.trimmed().isEmpty() || line_name.trimmed().isEmpty()) {
        return {};
    }

    const auto& stream_lines = edit_session.stream_lines(stream_name);
    const int line_count = std::max(
        1,
        static_cast<int>(std::count_if(
            stream_lines.cbegin(), stream_lines.cend(),
            [](const stream_cell::line_instance& line_value) {
                return line_value.enabled;
            }
        ))
    );
    int enabled_line_index = 0;
    for (int line_index = 0; line_index < static_cast<int>(stream_lines.size());
         line_index += 1) {
        const auto& line_value = stream_lines[static_cast<size_t>(line_index)];
        if (!line_value.enabled) {
            continue;
        }
        if (line_value.template_name.trimmed() != line_name.trimmed()) {
            enabled_line_index += 1;
            continue;
        }

        const QString color_mode_id
            = normalized_line_color_mode_id(line_value.color_mode_id);
        if (color_mode_id == QStringLiteral("manual")
            && line_value.color.isValid()) {
            return line_value.color;
        }

        return auto_palette_line_color(enabled_line_index, line_count);
    }

    return {};
}

QColor stream_controller::resolved_overlay_line_color(
    const QString& stream_name, const QString& line_name,
    const QColor& fallback_color
) const {
    if (stream_name.trimmed().isEmpty() || line_name.trimmed().isEmpty()) {
        return fallback_color;
    }

    const auto& stream_lines = edit_session.stream_lines(stream_name);
    const int line_count = std::max(
        1,
        static_cast<int>(std::count_if(
            stream_lines.cbegin(), stream_lines.cend(),
            [](const stream_cell::line_instance& line_value) {
                return line_value.enabled;
            }
        ))
    );
    int enabled_line_index = 0;
    for (int line_index = 0; line_index < static_cast<int>(stream_lines.size());
         line_index += 1) {
        const auto& line_value = stream_lines[static_cast<size_t>(line_index)];
        if (!line_value.enabled) {
            continue;
        }
        if (line_value.template_name.trimmed() != line_name.trimmed()) {
            enabled_line_index += 1;
            continue;
        }

        const QString color_mode_id
            = normalized_line_color_mode_id(line_value.color_mode_id);
        if (color_mode_id == QStringLiteral("negative_auto")) {
            return fallback_color;
        }
        if (color_mode_id == QStringLiteral("manual")
            && line_value.color.isValid()) {
            return line_value.color;
        }

        return auto_palette_line_color(enabled_line_index, line_count);
    }

    return fallback_color;
}
