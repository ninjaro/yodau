#include "shell/frontend_log.hpp"

#include <QStringList>

namespace frontend_log_support {

QString format_release_log_entry(const frontend_log_entry& entry) {
    QStringList parts;
    parts << QString("[%1]").arg(entry.timestamp.toString("HH:mm:ss"));
    parts << frontend_log_severity_name(entry.severity);

    if (!entry.stream_name.isEmpty()) {
        parts << entry.stream_name;
    }

    if (!entry.message.isEmpty()) {
        parts << entry.message;
    }

    return parts.join(' ');
}

QString format_debug_log_entry(const frontend_log_entry& entry) {
    QStringList parts;
    parts << QString("[%1]").arg(entry.timestamp.toString("HH:mm:ss.zzz"));
    parts << frontend_log_severity_name(entry.severity);
    parts << QString("area=%1").arg(frontend_log_area_name(entry.area));

    if (!entry.subsystem.isEmpty()) {
        parts << QString("subsystem=%1").arg(entry.subsystem);
    }

    if (!entry.stream_name.isEmpty()) {
        parts << QString("stream=%1").arg(entry.stream_name);
    }

    if (!entry.algorithm_id.isEmpty()) {
        parts << QString("alg=%1").arg(entry.algorithm_id);
    }

    if (!entry.message.isEmpty()) {
        parts << entry.message;
    }

    if (!entry.detail.isEmpty()) {
        parts << QString("detail=%1").arg(entry.detail);
    }

    return parts.join(' ');
}

} // namespace frontend_log_support

QString frontend_log_area_name(const frontend_log_area area) {
    switch (area) {
    case frontend_log_area::add:
        return QStringLiteral("add");
    case frontend_log_area::streams:
        return QStringLiteral("streams");
    case frontend_log_area::active:
    default:
        return QStringLiteral("active");
    }
}

QString frontend_log_severity_name(const frontend_log_severity severity) {
    switch (severity) {
    case frontend_log_severity::debug:
        return QStringLiteral("debug");
    case frontend_log_severity::info:
        return QStringLiteral("info");
    case frontend_log_severity::warning:
        return QStringLiteral("warn");
    case frontend_log_severity::error:
    default:
        return QStringLiteral("error");
    }
}

QString frontend_log_mode_name(const frontend_log_mode mode) {
    switch (mode) {
    case frontend_log_mode::debug:
        return QStringLiteral("debug");
    case frontend_log_mode::release:
    default:
        return QStringLiteral("release");
    }
}

QString format_frontend_log_entry(
    const frontend_log_mode mode, const frontend_log_entry& entry
) {
    switch (mode) {
    case frontend_log_mode::debug:
        return frontend_log_support::format_debug_log_entry(entry);
    case frontend_log_mode::release:
    default:
        return frontend_log_support::format_release_log_entry(entry);
    }
}

frontend_log_buffer::frontend_log_buffer(QObject* parent)
    : QObject(parent) { }

const QVector<frontend_log_entry>& frontend_log_buffer::entries() const {
    return entry_list;
}

QVector<frontend_log_entry>
frontend_log_buffer::entries_for_area(const frontend_log_area area) const {
    QVector<frontend_log_entry> filtered_entries;
    filtered_entries.reserve(entry_list.size());

    for (const frontend_log_entry& entry : entry_list) {
        if (entry.area == area) {
            filtered_entries.push_back(entry);
        }
    }

    return filtered_entries;
}

void frontend_log_buffer::append(frontend_log_entry entry) {
    if (!entry.timestamp.isValid()) {
        entry.timestamp = QDateTime::currentDateTime();
    }

    entry_list.push_back(entry);
    emit entry_appended(entry);
}

void frontend_log_buffer::clear() {
    entry_list.clear();
    emit cleared();
}
