#include "shell/app_log.hpp"

#include <QStringList>

namespace app_log_support {

QString format_release_log_entry(const app_log_entry& entry) {
    QStringList parts;
    parts << QString("[%1]").arg(entry.timestamp.toString("HH:mm:ss"));
    parts << app_log_severity_name(entry.severity);

    if (!entry.stream_name.isEmpty()) {
        parts << entry.stream_name;
    }

    if (!entry.event_type.isEmpty()) {
        parts << entry.event_type;
    }

    if (!entry.line_name.isEmpty()) {
        parts << QString("line=%1").arg(entry.line_name);
    }

    if (!entry.message.isEmpty()) {
        parts << entry.message;
    }

    return parts.join(' ');
}

QString format_debug_log_entry(const app_log_entry& entry) {
    QStringList parts;
    parts << QString("[%1]").arg(entry.timestamp.toString("HH:mm:ss.zzz"));
    parts << app_log_severity_name(entry.severity);
    parts << QString("area=%1").arg(app_log_area_name(entry.area));

    if (!entry.subsystem.isEmpty()) {
        parts << QString("subsystem=%1").arg(entry.subsystem);
    }

    if (!entry.stream_name.isEmpty()) {
        parts << QString("stream=%1").arg(entry.stream_name);
    }

    if (!entry.line_name.isEmpty()) {
        parts << QString("line=%1").arg(entry.line_name);
    }

    if (!entry.algorithm_id.isEmpty()) {
        parts << QString("alg=%1").arg(entry.algorithm_id);
    }

    if (!entry.event_type.isEmpty()) {
        parts << QString("event=%1").arg(entry.event_type);
    }

    if (!entry.message.isEmpty()) {
        parts << entry.message;
    }

    if (!entry.detail.isEmpty()) {
        parts << QString("detail=%1").arg(entry.detail);
    }

    return parts.join(' ');
}

} // namespace app_log_support

QString app_log_area_name(const app_log_area area) {
    switch (area) {
    case app_log_area::add:
        return QStringLiteral("add");
    case app_log_area::streams:
        return QStringLiteral("streams");
    case app_log_area::active:
    default:
        return QStringLiteral("active");
    }
}

QString app_log_severity_name(const app_log_severity severity) {
    switch (severity) {
    case app_log_severity::debug:
        return QStringLiteral("debug");
    case app_log_severity::info:
        return QStringLiteral("info");
    case app_log_severity::warning:
        return QStringLiteral("warn");
    case app_log_severity::error:
    default:
        return QStringLiteral("error");
    }
}

QString app_log_mode_name(const app_log_mode mode) {
    switch (mode) {
    case app_log_mode::debug:
        return QStringLiteral("debug");
    case app_log_mode::release:
    default:
        return QStringLiteral("release");
    }
}

QString format_app_log_entry(
    const app_log_mode mode, const app_log_entry& entry
) {
    switch (mode) {
    case app_log_mode::debug:
        return app_log_support::format_debug_log_entry(entry);
    case app_log_mode::release:
    default:
        return app_log_support::format_release_log_entry(entry);
    }
}

app_log_buffer::app_log_buffer(QObject* parent)
    : QObject(parent) { }

const QVector<app_log_entry>& app_log_buffer::entries() const {
    return entry_list;
}

QVector<app_log_entry>
app_log_buffer::entries_for_area(const app_log_area area) const {
    QVector<app_log_entry> filtered_entries;
    filtered_entries.reserve(entry_list.size());

    for (const app_log_entry& entry : entry_list) {
        if (entry.area == area) {
            filtered_entries.push_back(entry);
        }
    }

    return filtered_entries;
}

void app_log_buffer::append(app_log_entry entry) {
    if (!entry.timestamp.isValid()) {
        entry.timestamp = QDateTime::currentDateTime();
    }

    entry_list.push_back(entry);
    emit entry_appended(entry);
}

void app_log_buffer::clear() {
    entry_list.clear();
    emit cleared();
}
