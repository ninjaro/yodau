#include "widgets/log_toolbar_panel.hpp"

#include "shell/app_settings.hpp"
#include "shell/str_label.hpp"

#include <QComboBox>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QTextStream>

#include <algorithm>

namespace log_toolbar_panel_support {

QString filter_all_text() { return str_label("all"); }

QString log_area_title(const app_log_area area) {
    switch (area) {
    case app_log_area::add:
        return QStringLiteral("add");
    case app_log_area::streams:
        return QStringLiteral("streams");
    case app_log_area::active:
        return QStringLiteral("active");
    }

    return QStringLiteral("active");
}

QString report_scope_text(const std::optional<app_log_area> area) {
    return area.has_value() ? log_area_title(*area) : QStringLiteral("all");
}

void replace_filter_combo_items(
    QComboBox* combo, const QStringList& values, const QString& selected_value
) {
    if (combo == nullptr) {
        return;
    }

    QSignalBlocker blocker(combo);
    combo->clear();
    combo->addItem(filter_all_text(), QString());

    for (const QString& value : values) {
        combo->addItem(value, value);
    }

    const int selected_index
        = selected_value.isEmpty() ? 0 : combo->findData(selected_value);
    combo->setCurrentIndex(selected_index >= 0 ? selected_index : 0);
}

void insert_filter_combo_item(QComboBox* combo, const QString& raw_value) {
    if (combo == nullptr) {
        return;
    }
    const QString value = raw_value.trimmed();
    if (value.isEmpty() || combo->findData(value) >= 0) {
        return;
    }

    QSignalBlocker blocker(combo);
    int insertion_index = 1;
    while (insertion_index < combo->count()
           && combo->itemText(insertion_index).localeAwareCompare(value) < 0) {
        ++insertion_index;
    }
    combo->insertItem(insertion_index, value, value);
}

QString entry_search_haystack(const app_log_entry& entry) {
    QStringList parts;
    parts << app_log_area_name(entry.area)
          << app_log_severity_name(entry.severity) << entry.subsystem
          << entry.stream_name << entry.line_name << entry.algorithm_id
          << entry.event_type << entry.message << entry.detail;
    return parts.join(' ');
}

QString tsv_value(QString value) {
    value.replace('\t', ' ');
    value.replace('\n', ' ');
    value.replace('\r', ' ');
    return value;
}

QString severity_filter_text(const int severity_filter) {
    return severity_filter < 0
        ? filter_all_text()
        : app_log_severity_name(static_cast<app_log_severity>(severity_filter));
}

QJsonObject filters_json(
    const app_log_mode mode, const int area_filter, const int severity_filter,
    const QString& event_filter, const QString& line_filter,
    const QString& algorithm_filter, const QString& stream_filter,
    const QString& subsystem_filter, const QString& search_filter
) {
    QJsonObject object;
    object.insert(QStringLiteral("mode"), app_log_mode_name(mode));
    object.insert(
        QStringLiteral("area"),
        area_filter < 0 ? filter_all_text()
                        : log_area_title(static_cast<app_log_area>(area_filter))
    );
    object.insert(
        QStringLiteral("severity"), severity_filter_text(severity_filter)
    );
    object.insert(
        QStringLiteral("event"),
        event_filter.isEmpty() ? filter_all_text() : event_filter
    );
    object.insert(
        QStringLiteral("line_or_region"),
        line_filter.isEmpty() ? filter_all_text() : line_filter
    );
    object.insert(
        QStringLiteral("algorithm"),
        algorithm_filter.isEmpty() ? filter_all_text() : algorithm_filter
    );
    object.insert(
        QStringLiteral("stream"),
        stream_filter.isEmpty() ? filter_all_text() : stream_filter
    );
    object.insert(
        QStringLiteral("subsystem"),
        subsystem_filter.isEmpty() ? filter_all_text() : subsystem_filter
    );
    object.insert(
        QStringLiteral("search"),
        search_filter.isEmpty() ? filter_all_text() : search_filter
    );
    return object;
}

QJsonObject entry_json(const app_log_entry& entry) {
    QJsonObject object;
    object.insert(
        QStringLiteral("timestamp"),
        entry.timestamp.isValid() ? entry.timestamp.toString(Qt::ISODateWithMs)
                                  : QString()
    );
    object.insert(QStringLiteral("area"), app_log_area_name(entry.area));
    object.insert(
        QStringLiteral("severity"), app_log_severity_name(entry.severity)
    );
    object.insert(QStringLiteral("subsystem"), entry.subsystem);
    object.insert(QStringLiteral("stream_name"), entry.stream_name);
    object.insert(QStringLiteral("line_name"), entry.line_name);
    object.insert(QStringLiteral("algorithm_id"), entry.algorithm_id);
    object.insert(QStringLiteral("event_type"), entry.event_type);
    object.insert(QStringLiteral("message"), entry.message);
    object.insert(QStringLiteral("detail"), entry.detail);
    object.insert(
        QStringLiteral("line_color"),
        entry.line_color.isValid() ? entry.line_color.name(QColor::HexArgb)
                                   : QString()
    );
    object.insert(
        QStringLiteral("formatted_release"),
        format_app_log_entry(app_log_mode::release, entry)
    );
    object.insert(
        QStringLiteral("formatted_debug"),
        format_app_log_entry(app_log_mode::debug, entry)
    );
    return object;
}

} // namespace log_toolbar_panel_support

log_toolbar_panel::log_toolbar_panel(QWidget* parent)
    : QWidget(parent) {
    build_ui();
    refresh_filter_options();
}

void log_toolbar_panel::set_log_buffer(app_log_buffer* buffer) {
    if (shared_log_buffer == buffer) {
        refresh_filter_options();
        emit view_state_changed();
        return;
    }

    if (shared_log_buffer != nullptr) {
        disconnect(shared_log_buffer, nullptr, this, nullptr);
    }

    shared_log_buffer = buffer;

    if (shared_log_buffer != nullptr) {
        connect(
            shared_log_buffer, &app_log_buffer::entry_appended, this,
            [this](const app_log_entry& entry) {
                append_filter_options(entry);
                emit entry_available(entry);
            }
        );
        connect(
            shared_log_buffer, &app_log_buffer::entries_pruned, this,
            [this](qsizetype) {
                refresh_filter_options();
                emit view_state_changed();
            }
        );
        connect(shared_log_buffer, &app_log_buffer::cleared, this, [this]() {
            refresh_filter_options();
            emit view_state_changed();
        });
    }

    refresh_filter_options();
    emit view_state_changed();
}

bool log_toolbar_panel::append_entry(app_log_entry entry) const {
    if (shared_log_buffer == nullptr) {
        return false;
    }

    shared_log_buffer->append(std::move(entry));
    return true;
}

void log_toolbar_panel::set_log_mode(const app_log_mode mode) {
    const bool changed = current_log_mode != mode;
    current_log_mode = mode;

    if (log_mode_combo != nullptr) {
        QSignalBlocker blocker(log_mode_combo);
        log_mode_combo->setCurrentIndex(mode == app_log_mode::release ? 0 : 1);
    }

    emit view_state_changed();
    if (changed) {
        emit log_mode_changed(current_log_mode);
    }
}

app_log_mode log_toolbar_panel::log_mode() const { return current_log_mode; }

bool log_toolbar_panel::entry_matches(const app_log_entry& entry) const {
    const int active_area_filter = log_area_filter_combo != nullptr
        ? log_area_filter_combo->currentData().toInt()
        : current_log_area_filter;
    const int active_severity_filter = log_severity_filter_combo != nullptr
        ? log_severity_filter_combo->currentData().toInt()
        : current_log_severity_filter;
    const QString active_event_filter = log_event_filter_combo != nullptr
        ? log_event_filter_combo->currentData().toString()
        : current_log_event_filter;
    const QString active_line_filter = log_line_filter_combo != nullptr
        ? log_line_filter_combo->currentData().toString()
        : current_log_line_filter;
    const QString active_algorithm_filter
        = log_algorithm_filter_combo != nullptr
        ? log_algorithm_filter_combo->currentData().toString()
        : current_log_algorithm_filter;
    const QString active_stream_filter = log_stream_filter_combo != nullptr
        ? log_stream_filter_combo->currentData().toString()
        : current_log_stream_filter;
    const QString active_subsystem_filter
        = log_subsystem_filter_combo != nullptr
        ? log_subsystem_filter_combo->currentData().toString()
        : current_log_subsystem_filter;
    const QString active_search_filter = log_search_filter_edit != nullptr
        ? log_search_filter_edit->text().trimmed()
        : current_log_search_filter;

    if (active_area_filter >= 0
        && entry.area != static_cast<app_log_area>(active_area_filter)) {
        return false;
    }

    if (current_log_mode == app_log_mode::release
        && entry.severity == app_log_severity::debug) {
        return false;
    }

    if (active_severity_filter >= 0
        && static_cast<int>(entry.severity) != active_severity_filter) {
        return false;
    }

    if (!active_event_filter.isEmpty()
        && entry.event_type != active_event_filter) {
        return false;
    }

    if (!active_line_filter.isEmpty()
        && entry.line_name != active_line_filter) {
        return false;
    }

    if (!active_algorithm_filter.isEmpty()
        && entry.algorithm_id != active_algorithm_filter) {
        return false;
    }

    if (!active_stream_filter.isEmpty()
        && entry.stream_name != active_stream_filter) {
        return false;
    }

    if (!active_subsystem_filter.isEmpty()
        && entry.subsystem != active_subsystem_filter) {
        return false;
    }

    if (!active_search_filter.isEmpty()
        && !log_toolbar_panel_support::entry_search_haystack(entry).contains(
            active_search_filter, Qt::CaseInsensitive
        )) {
        return false;
    }

    return true;
}

QStringList log_toolbar_panel::formatted_entries(
    const std::optional<app_log_area> area
) const {
    QStringList lines;
    for (const app_log_entry& entry : filtered_entries(area)) {
        lines.push_back(format_app_log_entry(current_log_mode, entry));
    }
    return lines;
}

QVector<app_log_entry> log_toolbar_panel::filtered_entries(
    const std::optional<app_log_area> area
) const {
    QVector<app_log_entry> entries;
    if (shared_log_buffer == nullptr) {
        return entries;
    }

    entries.reserve(shared_log_buffer->entries().size());
    for (const app_log_entry& entry : shared_log_buffer->entries()) {
        if (area.has_value() && entry.area != *area) {
            continue;
        }
        if (!entry_matches(entry)) {
            continue;
        }
        entries.push_back(entry);
    }

    return entries;
}

QString log_toolbar_panel::compose_log_report(
    const std::optional<app_log_area> area
) const {
    QStringList lines;
    lines << QStringLiteral("yodau log report");
    lines
        << QStringLiteral(
               "scope=%1 mode=%2 area_filter=%3 severity=%4 event=%5 line=%6 "
               "algorithm=%7 stream=%8 subsystem=%9 search=%10"
           )
               .arg(log_toolbar_panel_support::report_scope_text(area))
               .arg(
                   current_log_mode == app_log_mode::release
                       ? QStringLiteral("release")
                       : QStringLiteral("debug")
               )
               .arg(
                   current_log_area_filter < 0
                       ? log_toolbar_panel_support::filter_all_text()
                       : log_toolbar_panel_support::log_area_title(
                             static_cast<app_log_area>(current_log_area_filter)
                         )
               )
               .arg(
                   log_toolbar_panel_support::severity_filter_text(
                       current_log_severity_filter
                   )
               )
               .arg(
                   current_log_event_filter.isEmpty()
                       ? log_toolbar_panel_support::filter_all_text()
                       : current_log_event_filter
               )
               .arg(
                   current_log_line_filter.isEmpty()
                       ? log_toolbar_panel_support::filter_all_text()
                       : current_log_line_filter
               )
               .arg(
                   current_log_algorithm_filter.isEmpty()
                       ? log_toolbar_panel_support::filter_all_text()
                       : current_log_algorithm_filter
               )
               .arg(
                   current_log_stream_filter.isEmpty()
                       ? log_toolbar_panel_support::filter_all_text()
                       : current_log_stream_filter
               )
               .arg(
                   current_log_subsystem_filter.isEmpty()
                       ? log_toolbar_panel_support::filter_all_text()
                       : current_log_subsystem_filter
               )
               .arg(
                   current_log_search_filter.isEmpty()
                       ? log_toolbar_panel_support::filter_all_text()
                       : current_log_search_filter
               );

    const QStringList visible_entries = formatted_entries(area);
    lines.append(visible_entries);

    if (visible_entries.isEmpty()) {
        lines << QStringLiteral("(no matching entries)");
    }

    return lines.join('\n');
}

QString log_toolbar_panel::compose_log_summary(
    const std::optional<app_log_area> area
) const {
    int debug_count = 0;
    int info_count = 0;
    int warning_count = 0;
    int error_count = 0;
    QSet<QString> visible_streams;
    QSet<QString> visible_subsystems;
    QSet<QString> visible_events;
    QSet<QString> visible_lines;
    QSet<QString> visible_algorithms;

    for (const app_log_entry& entry : filtered_entries(area)) {
        switch (entry.severity) {
        case app_log_severity::debug:
            debug_count += 1;
            break;
        case app_log_severity::info:
            info_count += 1;
            break;
        case app_log_severity::warning:
            warning_count += 1;
            break;
        case app_log_severity::error:
            error_count += 1;
            break;
        }

        if (!entry.stream_name.trimmed().isEmpty()) {
            visible_streams.insert(entry.stream_name.trimmed());
        }
        if (!entry.subsystem.trimmed().isEmpty()) {
            visible_subsystems.insert(entry.subsystem.trimmed());
        }
        if (!entry.event_type.trimmed().isEmpty()) {
            visible_events.insert(entry.event_type.trimmed());
        }
        if (!entry.line_name.trimmed().isEmpty()) {
            visible_lines.insert(entry.line_name.trimmed());
        }
        if (!entry.algorithm_id.trimmed().isEmpty()) {
            visible_algorithms.insert(entry.algorithm_id.trimmed());
        }
    }

    QStringList lines;
    lines << QStringLiteral("yodau log summary");
    lines << QStringLiteral("scope=%1")
                 .arg(log_toolbar_panel_support::report_scope_text(area));
    lines << QStringLiteral("entries=%1 debug=%2 info=%3 warn=%4 error=%5")
                 .arg(debug_count + info_count + warning_count + error_count)
                 .arg(debug_count)
                 .arg(info_count)
                 .arg(warning_count)
                 .arg(error_count);
    lines << QStringLiteral(
                 "streams=%1 subsystems=%2 events=%3 lines=%4 algorithms=%5"
    )
                 .arg(visible_streams.size())
                 .arg(visible_subsystems.size())
                 .arg(visible_events.size())
                 .arg(visible_lines.size())
                 .arg(visible_algorithms.size());

    QStringList stream_list = visible_streams.values();
    QStringList subsystem_list = visible_subsystems.values();
    QStringList event_list = visible_events.values();
    QStringList line_list = visible_lines.values();
    QStringList algorithm_list = visible_algorithms.values();
    std::sort(
        stream_list.begin(), stream_list.end(),
        [](const QString& lhs, const QString& rhs) {
            return lhs.localeAwareCompare(rhs) < 0;
        }
    );
    std::sort(
        subsystem_list.begin(), subsystem_list.end(),
        [](const QString& lhs, const QString& rhs) {
            return lhs.localeAwareCompare(rhs) < 0;
        }
    );
    std::sort(
        event_list.begin(), event_list.end(),
        [](const QString& lhs, const QString& rhs) {
            return lhs.localeAwareCompare(rhs) < 0;
        }
    );
    std::sort(
        line_list.begin(), line_list.end(),
        [](const QString& lhs, const QString& rhs) {
            return lhs.localeAwareCompare(rhs) < 0;
        }
    );
    std::sort(
        algorithm_list.begin(), algorithm_list.end(),
        [](const QString& lhs, const QString& rhs) {
            return lhs.localeAwareCompare(rhs) < 0;
        }
    );

    if (!visible_streams.isEmpty()) {
        lines << QStringLiteral("stream_list=%1").arg(stream_list.join(','));
    }
    if (!visible_subsystems.isEmpty()) {
        lines << QStringLiteral("subsystem_list=%1")
                     .arg(subsystem_list.join(','));
    }
    if (!visible_events.isEmpty()) {
        lines << QStringLiteral("event_list=%1").arg(event_list.join(','));
    }
    if (!visible_lines.isEmpty()) {
        lines << QStringLiteral("line_list=%1").arg(line_list.join(','));
    }
    if (!visible_algorithms.isEmpty()) {
        lines << QStringLiteral("algorithm_list=%1")
                     .arg(algorithm_list.join(','));
    }

    return lines.join('\n');
}

bool log_toolbar_panel::write_log_report(
    const std::optional<app_log_area> area, const QString& path
) const {
    if (path.trimmed().isEmpty()) {
        return false;
    }

    const QVector<app_log_entry> entries = filtered_entries(area);
    const QString suffix = QFileInfo(path).suffix().trimmed().toLower();

    QFile file(path);
    if (!file.open(
            QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate
        )) {
        return false;
    }

    if (suffix == QStringLiteral("json")) {
        QJsonArray entry_array;
        for (const app_log_entry& entry : entries) {
            entry_array.push_back(log_toolbar_panel_support::entry_json(entry));
        }

        QJsonObject root;
        root.insert(
            QStringLiteral("report"), QStringLiteral("yodau log report")
        );
        root.insert(
            QStringLiteral("scope"),
            log_toolbar_panel_support::report_scope_text(area)
        );
        root.insert(
            QStringLiteral("filters"),
            log_toolbar_panel_support::filters_json(
                current_log_mode, current_log_area_filter,
                current_log_severity_filter, current_log_event_filter,
                current_log_line_filter, current_log_algorithm_filter,
                current_log_stream_filter, current_log_subsystem_filter,
                current_log_search_filter
            )
        );
        root.insert(QStringLiteral("entry_count"), entries.size());
        root.insert(QStringLiteral("entries"), entry_array);
        return file.write(QJsonDocument(root).toJson(QJsonDocument::Indented))
            >= 0;
    }

    QTextStream stream(&file);
    if (suffix == QStringLiteral("tsv")) {
        stream << QStringLiteral(
            "timestamp\tarea\tseverity\tsubsystem\tstream_name\tline_"
            "name\tevent_type\talgorithm_id\tmessage\tdetail\tline_color\n"
        );
        for (const app_log_entry& entry : entries) {
            stream << log_toolbar_panel_support::tsv_value(
                entry.timestamp.isValid()
                    ? entry.timestamp.toString(Qt::ISODateWithMs)
                    : QString()
            ) << '\t'
                   << log_toolbar_panel_support::tsv_value(
                          app_log_area_name(entry.area)
                      )
                   << '\t'
                   << log_toolbar_panel_support::tsv_value(
                          app_log_severity_name(entry.severity)
                      )
                   << '\t'
                   << log_toolbar_panel_support::tsv_value(entry.subsystem)
                   << '\t'
                   << log_toolbar_panel_support::tsv_value(entry.stream_name)
                   << '\t'
                   << log_toolbar_panel_support::tsv_value(entry.line_name)
                   << '\t'
                   << log_toolbar_panel_support::tsv_value(entry.event_type)
                   << '\t'
                   << log_toolbar_panel_support::tsv_value(entry.algorithm_id)
                   << '\t'
                   << log_toolbar_panel_support::tsv_value(entry.message)
                   << '\t' << log_toolbar_panel_support::tsv_value(entry.detail)
                   << '\t'
                   << log_toolbar_panel_support::tsv_value(
                          entry.line_color.isValid()
                              ? entry.line_color.name(QColor::HexArgb)
                              : QString()
                      )
                   << '\n';
        }
        return stream.status() == QTextStream::Ok;
    }

    stream << compose_log_report(area);
    return stream.status() == QTextStream::Ok;
}

void log_toolbar_panel::on_log_mode_changed(const int index) {
    set_log_mode(index == 0 ? app_log_mode::release : app_log_mode::debug);
}

void log_toolbar_panel::on_log_area_filter_changed(const int index) {
    Q_UNUSED(index);

    if (log_area_filter_combo == nullptr) {
        return;
    }

    current_log_area_filter = log_area_filter_combo->currentData().toInt();
    emit view_state_changed();
}

void log_toolbar_panel::on_log_severity_filter_changed(const int index) {
    Q_UNUSED(index);

    if (log_severity_filter_combo == nullptr) {
        return;
    }

    current_log_severity_filter
        = log_severity_filter_combo->currentData().toInt();
    emit view_state_changed();
}

void log_toolbar_panel::on_log_event_filter_changed(const int index) {
    Q_UNUSED(index);

    if (log_event_filter_combo == nullptr) {
        return;
    }

    current_log_event_filter = log_event_filter_combo->currentData().toString();
    emit view_state_changed();
}

void log_toolbar_panel::on_log_line_filter_changed(const int index) {
    Q_UNUSED(index);

    if (log_line_filter_combo == nullptr) {
        return;
    }

    current_log_line_filter = log_line_filter_combo->currentData().toString();
    emit view_state_changed();
}

void log_toolbar_panel::on_log_algorithm_filter_changed(const int index) {
    Q_UNUSED(index);

    if (log_algorithm_filter_combo == nullptr) {
        return;
    }

    current_log_algorithm_filter
        = log_algorithm_filter_combo->currentData().toString();
    emit view_state_changed();
}

void log_toolbar_panel::on_log_stream_filter_changed(const int index) {
    Q_UNUSED(index);

    if (log_stream_filter_combo == nullptr) {
        return;
    }

    current_log_stream_filter
        = log_stream_filter_combo->currentData().toString();
    emit view_state_changed();
}

void log_toolbar_panel::on_log_subsystem_filter_changed(const int index) {
    Q_UNUSED(index);

    if (log_subsystem_filter_combo == nullptr) {
        return;
    }

    current_log_subsystem_filter
        = log_subsystem_filter_combo->currentData().toString();
    emit view_state_changed();
}

void log_toolbar_panel::on_log_search_filter_changed(const QString& text) {
    current_log_search_filter = text.trimmed();
    emit view_state_changed();
}

void log_toolbar_panel::build_ui() {
    const auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    const auto label = new QLabel(str_label("log mode"), this);
    log_mode_combo = new QComboBox(this);
    log_mode_combo->setObjectName(QStringLiteral("settings_log_mode_combo"));
    log_mode_combo->addItem(str_label("release"));
    log_mode_combo->addItem(str_label("debug"));
    log_mode_combo->setCurrentIndex(
        current_log_mode == app_log_mode::release ? 0 : 1
    );

    const auto area_label = new QLabel(str_label("area"), this);
    log_area_filter_combo = new QComboBox(this);
    log_area_filter_combo->setObjectName(
        QStringLiteral("settings_log_area_filter_combo")
    );
    log_area_filter_combo->addItem(
        log_toolbar_panel_support::filter_all_text(), -1
    );
    log_area_filter_combo->addItem(
        str_label("add"), static_cast<int>(app_log_area::add)
    );
    log_area_filter_combo->addItem(
        str_label("streams"), static_cast<int>(app_log_area::streams)
    );
    log_area_filter_combo->addItem(
        str_label("active"), static_cast<int>(app_log_area::active)
    );

    const auto severity_label = new QLabel(str_label("severity"), this);
    log_severity_filter_combo = new QComboBox(this);
    log_severity_filter_combo->setObjectName(
        QStringLiteral("settings_log_severity_filter_combo")
    );
    log_severity_filter_combo->addItem(
        log_toolbar_panel_support::filter_all_text(), -1
    );
    log_severity_filter_combo->addItem(
        app_log_severity_name(app_log_severity::debug),
        static_cast<int>(app_log_severity::debug)
    );
    log_severity_filter_combo->addItem(
        app_log_severity_name(app_log_severity::info),
        static_cast<int>(app_log_severity::info)
    );
    log_severity_filter_combo->addItem(
        app_log_severity_name(app_log_severity::warning),
        static_cast<int>(app_log_severity::warning)
    );
    log_severity_filter_combo->addItem(
        app_log_severity_name(app_log_severity::error),
        static_cast<int>(app_log_severity::error)
    );

    const auto event_label = new QLabel(str_label("event"), this);
    log_event_filter_combo = new QComboBox(this);
    log_event_filter_combo->setObjectName(
        QStringLiteral("settings_log_event_filter_combo")
    );

    const auto line_label = new QLabel(str_label("line/region"), this);
    log_line_filter_combo = new QComboBox(this);
    log_line_filter_combo->setObjectName(
        QStringLiteral("settings_log_line_filter_combo")
    );

    const auto algorithm_label = new QLabel(str_label("algorithm"), this);
    log_algorithm_filter_combo = new QComboBox(this);
    log_algorithm_filter_combo->setObjectName(
        QStringLiteral("settings_log_algorithm_filter_combo")
    );

    const auto stream_label = new QLabel(str_label("stream"), this);
    log_stream_filter_combo = new QComboBox(this);
    log_stream_filter_combo->setObjectName(
        QStringLiteral("settings_log_stream_filter_combo")
    );

    const auto subsystem_label = new QLabel(str_label("subsystem"), this);
    log_subsystem_filter_combo = new QComboBox(this);
    log_subsystem_filter_combo->setObjectName(
        QStringLiteral("settings_log_subsystem_filter_combo")
    );

    const auto search_label = new QLabel(str_label("search"), this);
    log_search_filter_edit = new QLineEdit(this);
    log_search_filter_edit->setObjectName(
        QStringLiteral("settings_log_search_filter_edit")
    );
    log_search_filter_edit->setPlaceholderText(
        str_label("stream, line, event, message...")
    );
    log_search_filter_edit->setClearButtonEnabled(true);
    log_search_filter_edit->setMinimumWidth(180);

    copy_logs_btn = new QPushButton(str_label("copy logs"), this);
    copy_logs_btn->setObjectName(QStringLiteral("settings_copy_logs_button"));
    copy_summary_btn = new QPushButton(str_label("copy summary"), this);
    copy_summary_btn->setObjectName(
        QStringLiteral("settings_copy_summary_button")
    );
    save_logs_btn = new QPushButton(str_label("save logs"), this);
    save_logs_btn->setObjectName(QStringLiteral("settings_save_logs_button"));

    layout->addWidget(label);
    layout->addWidget(log_mode_combo);
    layout->addWidget(area_label);
    layout->addWidget(log_area_filter_combo);
    layout->addWidget(severity_label);
    layout->addWidget(log_severity_filter_combo);
    layout->addWidget(event_label);
    layout->addWidget(log_event_filter_combo);
    layout->addWidget(line_label);
    layout->addWidget(log_line_filter_combo);
    layout->addWidget(algorithm_label);
    layout->addWidget(log_algorithm_filter_combo);
    layout->addWidget(stream_label);
    layout->addWidget(log_stream_filter_combo);
    layout->addWidget(subsystem_label);
    layout->addWidget(log_subsystem_filter_combo);
    layout->addWidget(search_label);
    layout->addWidget(log_search_filter_edit, 1);
    layout->addWidget(copy_logs_btn);
    layout->addWidget(copy_summary_btn);
    layout->addWidget(save_logs_btn);

    connect(
        log_mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &log_toolbar_panel::on_log_mode_changed
    );
    connect(
        log_area_filter_combo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &log_toolbar_panel::on_log_area_filter_changed
    );
    connect(
        log_severity_filter_combo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &log_toolbar_panel::on_log_severity_filter_changed
    );
    connect(
        log_event_filter_combo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &log_toolbar_panel::on_log_event_filter_changed
    );
    connect(
        log_line_filter_combo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &log_toolbar_panel::on_log_line_filter_changed
    );
    connect(
        log_algorithm_filter_combo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &log_toolbar_panel::on_log_algorithm_filter_changed
    );
    connect(
        log_stream_filter_combo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &log_toolbar_panel::on_log_stream_filter_changed
    );
    connect(
        log_subsystem_filter_combo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &log_toolbar_panel::on_log_subsystem_filter_changed
    );
    connect(
        log_search_filter_edit, &QLineEdit::textChanged, this,
        &log_toolbar_panel::on_log_search_filter_changed
    );
    connect(
        copy_logs_btn, &QPushButton::clicked, this,
        &log_toolbar_panel::copy_logs_requested
    );
    connect(
        copy_summary_btn, &QPushButton::clicked, this,
        &log_toolbar_panel::copy_summary_requested
    );
    connect(
        save_logs_btn, &QPushButton::clicked, this,
        &log_toolbar_panel::save_logs_requested
    );
}

void log_toolbar_panel::refresh_filter_options() const {
    QSet<QString> event_name_set;
    QSet<QString> line_name_set;
    QSet<QString> algorithm_name_set;
    QSet<QString> stream_name_set;
    QSet<QString> subsystem_name_set;

    if (shared_log_buffer != nullptr) {
        for (const app_log_entry& entry : shared_log_buffer->entries()) {
            const QString event_name = entry.event_type.trimmed();
            if (!event_name.isEmpty()) {
                event_name_set.insert(event_name);
            }

            const QString stream_name = entry.stream_name.trimmed();
            if (!stream_name.isEmpty()) {
                stream_name_set.insert(stream_name);
            }

            const QString line_name = entry.line_name.trimmed();
            if (!line_name.isEmpty()) {
                line_name_set.insert(line_name);
            }

            const QString algorithm_name = entry.algorithm_id.trimmed();
            if (!algorithm_name.isEmpty()) {
                algorithm_name_set.insert(algorithm_name);
            }

            const QString subsystem_name = entry.subsystem.trimmed();
            if (!subsystem_name.isEmpty()) {
                subsystem_name_set.insert(subsystem_name);
            }
        }
    }

    QStringList event_names = event_name_set.values();
    QStringList line_names = line_name_set.values();
    QStringList algorithm_names = algorithm_name_set.values();
    QStringList stream_names = stream_name_set.values();
    QStringList subsystem_names = subsystem_name_set.values();

    std::sort(
        event_names.begin(), event_names.end(),
        [](const QString& lhs, const QString& rhs) {
            return lhs.localeAwareCompare(rhs) < 0;
        }
    );
    std::sort(
        line_names.begin(), line_names.end(),
        [](const QString& lhs, const QString& rhs) {
            return lhs.localeAwareCompare(rhs) < 0;
        }
    );
    std::sort(
        algorithm_names.begin(), algorithm_names.end(),
        [](const QString& lhs, const QString& rhs) {
            return lhs.localeAwareCompare(rhs) < 0;
        }
    );
    std::sort(
        stream_names.begin(), stream_names.end(),
        [](const QString& lhs, const QString& rhs) {
            return lhs.localeAwareCompare(rhs) < 0;
        }
    );
    std::sort(
        subsystem_names.begin(), subsystem_names.end(),
        [](const QString& lhs, const QString& rhs) {
            return lhs.localeAwareCompare(rhs) < 0;
        }
    );

    log_toolbar_panel_support::replace_filter_combo_items(
        log_event_filter_combo, event_names, current_log_event_filter
    );
    log_toolbar_panel_support::replace_filter_combo_items(
        log_line_filter_combo, line_names, current_log_line_filter
    );
    log_toolbar_panel_support::replace_filter_combo_items(
        log_algorithm_filter_combo, algorithm_names,
        current_log_algorithm_filter
    );
    log_toolbar_panel_support::replace_filter_combo_items(
        log_stream_filter_combo, stream_names, current_log_stream_filter
    );
    log_toolbar_panel_support::replace_filter_combo_items(
        log_subsystem_filter_combo, subsystem_names,
        current_log_subsystem_filter
    );
}

void log_toolbar_panel::append_filter_options(
    const app_log_entry& entry
) const {
    log_toolbar_panel_support::insert_filter_combo_item(
        log_event_filter_combo, entry.event_type
    );
    log_toolbar_panel_support::insert_filter_combo_item(
        log_line_filter_combo, entry.line_name
    );
    log_toolbar_panel_support::insert_filter_combo_item(
        log_algorithm_filter_combo, entry.algorithm_id
    );
    log_toolbar_panel_support::insert_filter_combo_item(
        log_stream_filter_combo, entry.stream_name
    );
    log_toolbar_panel_support::insert_filter_combo_item(
        log_subsystem_filter_combo, entry.subsystem
    );
}
